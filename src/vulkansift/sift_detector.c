#include "sift_detector.h"

#include "vkenv/logger.h"
#include "vkenv/vulkan_utils.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char LOG_TAG[] = "SiftDetector";

typedef struct
{
  uint32_t is_vertical;
  uint32_t array_layer;
  uint32_t kernel_size;
  float kernel[VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE];
} GaussianBlurPushConsts;

void getGPUDebugMarkerFuncs(vksift_SiftDetector detector)
{
  detector->vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(detector->dev->device, "VkDebugMarkerMarkerInfoEXT");
  detector->vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(detector->dev->device, "vkCmdDebugMarkerEndEXT");
  detector->debug_marker_supported = (detector->vkCmdDebugMarkerBeginEXT != NULL) && (detector->vkCmdDebugMarkerEndEXT != NULL);
}

void beginMarkerRegion(vksift_SiftDetector detector, VkCommandBuffer cmd_buf, const char *region_name)
{
  if (detector->debug_marker_supported)
  {
    VkDebugMarkerMarkerInfoEXT marker_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, .pMarkerName = region_name};
    detector->vkCmdDebugMarkerBeginEXT(cmd_buf, &marker_info);
  }
}
void endMarkerRegion(vksift_SiftDetector detector, VkCommandBuffer cmd_buf)
{
  if (detector->debug_marker_supported)
  {
    detector->vkCmdDebugMarkerEndEXT(cmd_buf);
  }
}

void setupGaussianKernels(vksift_SiftDetector detector)
{
  uint32_t nb_scales = detector->mem->nb_scales_per_octave;
  detector->gaussian_kernels = (float *)malloc(sizeof(float) * VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE * (nb_scales + 3));
  detector->gaussian_kernel_sizes = (uint32_t *)malloc(sizeof(uint32_t) * (nb_scales + 3));
  for (uint32_t i = 0; i < VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE * (nb_scales + 3); i++)
  {
    detector->gaussian_kernels[i] = 0.f;
  }
  memset(detector->gaussian_kernel_sizes, 0, sizeof(uint32_t) * (nb_scales + 3));

  for (uint32_t scale_i = 0; scale_i < (nb_scales + 3); scale_i++)
  {
    // The idea of the kernels in the scale space is that each scale is a blurred version of the previous scale.
    // The first scale of the pyramid (seed scale means scale 0 at octave 0), has a blur level defied by the configuration
    // (default to 1.6 from Lowe's paper). We take into account the initial blur effect of the input image (default 0.5) so the first kernel
    // perform a blurring operation to tranform the input image with defined blur level (0.5) to a seed image with defined blur level (1.6)
    // that will be used to sequentially build the scale space.
    // When building the scale-space, the idea if that each octave is 2x more blurred than the previous octave and every first scale of an octave
    // if a 2x downscaled version of the scale nb_scale (3) of the previous octave. Since the downscaling does not change the blur level, this means that
    // on the previous octave the scale nb_scale (3) is 2x more blurred than the scale 0 of the same octave. This sets the constraint that each
    // scale is 2^(1.0/nb_scale)*seed_scale_sigma more blurred that the previous one, since for the scale nb_scale 2^(nb_scale/nb_scale) = 2

    float sep_kernel_sigma;
    if (scale_i == 0)
    {
      // Used only for first octave (since all other first scales used downsampled scale from octave-1)
      // The seed scale initial blur level is doubled when 2x upsampling is used (basically account for the blur effect caused by the upsampling)
      float first_scale_init_blur_level = detector->mem->use_upsampling ? detector->input_blur_level * 2.f : detector->input_blur_level;
      sep_kernel_sigma = sqrtf((detector->seed_scale_sigma * detector->seed_scale_sigma) - (first_scale_init_blur_level * first_scale_init_blur_level));
    }
    else
    {
      // Gaussian blur from one scale to the other
      float sig_prev = powf(powf(2.f, 1.f / nb_scales), (float)(scale_i - 1)) * detector->seed_scale_sigma;
      float sig_total = sig_prev * powf(2.f, 1.f / nb_scales);
      sep_kernel_sigma = sqrtf(sig_total * sig_total - sig_prev * sig_prev);
    }

    // Compute the gaussian kernel for the defined sigma value
    uint32_t kernel_size = (int)(ceilf(sep_kernel_sigma * 4.f) + 1.f);
    if (kernel_size > VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE)
    {
      kernel_size = VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE;
    }
    detector->gaussian_kernel_sizes[scale_i] = kernel_size;

    float kernel_tmp_data[VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE];
    kernel_tmp_data[0] = 1.f;
    float sum_kernel = kernel_tmp_data[0];
    for (uint32_t i = 1; i < kernel_size; i++)
    {
      kernel_tmp_data[i] = exp(-0.5 * powf((float)(i), 2.f) / powf(sep_kernel_sigma, 2.f));
      sum_kernel += 2 * kernel_tmp_data[i];
    }
    logError(LOG_TAG, "scale %d", scale_i);
    for (uint32_t i = 0; i < kernel_size; i++)
    {
      kernel_tmp_data[i] /= sum_kernel;
      logError(LOG_TAG, "%f", kernel_tmp_data[i]);
    }

    float *scale_kernel = &detector->gaussian_kernels[scale_i * VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE];
    // Compute hardware interpolated kernel if necessary
    if (detector->use_hardware_interp_kernel)
    {
      // Hardware interpolated kernel based on https://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
      // The goal here is to use hardware sampler to reduce the number of texture fetch by a factor of two
      // The same space is used in the kernel buffer, number of coeff is /2 but we add the texture offset to the buffer
      // First kernel coeff and offset stays the same
      scale_kernel[0] = kernel_tmp_data[0];
      scale_kernel[1] = (float)0;
      for (uint32_t data_idx = 1, kern_idx = 1; (data_idx + 1) < kernel_size; data_idx += 2, kern_idx++)
      {
        scale_kernel[kern_idx * 2] = kernel_tmp_data[data_idx] + kernel_tmp_data[data_idx + 1];
        scale_kernel[(kern_idx * 2) + 1] = (((float)data_idx * kernel_tmp_data[data_idx]) + ((float)(data_idx + 1) * kernel_tmp_data[data_idx + 1])) /
                                           (kernel_tmp_data[data_idx] + kernel_tmp_data[data_idx + 1]);
      }
    }
    else
    {
      for (uint32_t i = 0; i < kernel_size; i++)
      {
        scale_kernel[i] = kernel_tmp_data[i];
      }
    }
    logInfo(LOG_TAG, "Scale %d: sigma=%f | kernel size=%d", scale_i, sep_kernel_sigma, kernel_size);
  }
}

bool setupCommandPools(vksift_SiftDetector detector)
{
  VkCommandPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                       .queueFamilyIndex = detector->dev->general_queue_family_idx};
  if (vkCreateCommandPool(detector->dev->device, &pool_info, NULL, &detector->general_command_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create the general-purpose command pool");
    return false;
  }

  if (detector->dev->async_compute_available)
  {
    VkCommandPoolCreateInfo async_pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                               .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                               .queueFamilyIndex = detector->dev->async_compute_queue_family_idx};
    if (vkCreateCommandPool(detector->dev->device, &async_pool_info, NULL, &detector->async_compute_command_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create the asynchronous compute command pool");
      return false;
    }
  }

  return true;
}

bool allocateCommandBuffers(vksift_SiftDetector detector)
{
  if (detector->dev->async_compute_available)
  {
    // TODO manage async compute
    VkCommandBufferAllocateInfo allocate_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                 .pNext = NULL,
                                                 .commandPool = detector->general_command_pool,
                                                 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                 .commandBufferCount = 1};

    if (vkAllocateCommandBuffers(detector->dev->device, &allocate_info, &detector->sync_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate the sync command buffer on the general-purpose pool");
      return false;
    }
  }
  else
  {

    VkCommandBufferAllocateInfo allocate_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                 .pNext = NULL,
                                                 .commandPool = detector->general_command_pool,
                                                 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                 .commandBufferCount = 1};

    if (vkAllocateCommandBuffers(detector->dev->device, &allocate_info, &detector->sync_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate the sync command buffer on the general-purpose pool");
      return false;
    }
  }

  return true;
}

bool setupImageSampler(vksift_SiftDetector detector)
{
  VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                      .pNext = NULL,
                                      .flags = 0,
                                      .magFilter = VK_FILTER_LINEAR,
                                      .minFilter = VK_FILTER_LINEAR,
                                      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                      .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      .addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      .mipLodBias = 0.f,
                                      .anisotropyEnable = VK_FALSE,
                                      .maxAnisotropy = 0.f,
                                      .compareEnable = VK_FALSE,
                                      .compareOp = VK_COMPARE_OP_ALWAYS,
                                      .minLod = 0.f,
                                      .maxLod = 0.f,
                                      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                                      .unnormalizedCoordinates = VK_FALSE};

  if (vkCreateSampler(detector->dev->device, &sampler_info, NULL, &detector->image_sampler) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create image sampler");
    return false;
  }
  return true;
}

VkDescriptorSetLayout *allocMultLayoutCopy(VkDescriptorSetLayout layout, int nb_copy)
{
  VkDescriptorSetLayout *layout_arr = (VkDescriptorSetLayout *)malloc(sizeof(VkDescriptorSetLayout) * nb_copy);
  for (int i = 0; i < nb_copy; i++)
  {
    layout_arr[i] = layout;
  }
  return layout_arr;
}

bool prepareDescriptorSets(vksift_SiftDetector detector)
{
  VkResult alloc_res;
  ///////////////////////////////////////////////////
  // Descriptors for GaussianBlur pipeline
  ///////////////////////////////////////////////////
  VkDescriptorSetLayoutBinding blur_input_layout_binding = {.binding = 0,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding blur_output_layout_binding = {.binding = 1,
                                                             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                             .descriptorCount = 1,
                                                             .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                             .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding blur_bindings[2] = {blur_input_layout_binding, blur_output_layout_binding};
  VkDescriptorSetLayoutCreateInfo blur_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = blur_bindings};
  if (vkCreateDescriptorSetLayout(detector->dev->device, &blur_layout_info, NULL, &detector->blur_desc_set_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create GaussianBlur descriptor set layout");
    return false;
  }

  // Create descriptor pool to allocate descriptor sets (reserve x2 for the horizontal and vertical pass)
  VkDescriptorPoolSize blur_pool_sizes[2];
  blur_pool_sizes[0] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = detector->mem->max_nb_octaves * 2};
  blur_pool_sizes[1] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = detector->mem->max_nb_octaves * 2};
  VkDescriptorPoolCreateInfo blur_descriptor_pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                          .maxSets = detector->mem->max_nb_octaves * 2,
                                                          .poolSizeCount = 2,
                                                          .pPoolSizes = blur_pool_sizes};
  if (vkCreateDescriptorPool(detector->dev->device, &blur_descriptor_pool_info, NULL, &detector->blur_desc_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create GaussianBlur descriptor pool");
    return false;
  }

  // Create descriptor sets that can be bound in command buffer
  VkDescriptorSetLayout *blur_layouts = allocMultLayoutCopy(detector->blur_desc_set_layout, detector->mem->max_nb_octaves * 2);
  detector->blur_desc_sets = (VkDescriptorSet *)malloc(sizeof(VkDescriptorSet) * detector->mem->max_nb_octaves * 2);
  detector->blur_h_desc_sets = detector->blur_desc_sets;
  detector->blur_v_desc_sets = detector->blur_desc_sets + detector->mem->max_nb_octaves;
  VkDescriptorSetAllocateInfo blur_alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                 .descriptorPool = detector->blur_desc_pool,
                                                 .descriptorSetCount = detector->mem->max_nb_octaves * 2,
                                                 .pSetLayouts = blur_layouts};

  alloc_res = vkAllocateDescriptorSets(detector->dev->device, &blur_alloc_info, detector->blur_desc_sets);
  free(blur_layouts);
  if (alloc_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate GaussianBlur descriptor set");
    return false;
  }

  ///////////////////////////////////////////////////
  // Descriptors for DifferenceOfGaussian pipeline
  ///////////////////////////////////////////////////
  VkDescriptorSetLayoutBinding dog_input_layout_binding = {.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                           .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding dog_output_layout_binding = {.binding = 1,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding dog_bindings[2] = {dog_input_layout_binding, dog_output_layout_binding};

  VkDescriptorSetLayoutCreateInfo dog_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = dog_bindings};

  if (vkCreateDescriptorSetLayout(detector->dev->device, &dog_layout_info, NULL, &detector->dog_desc_set_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create DifferenceOfGaussian descriptor set layout");
    return false;
  }

  // Create descriptor pool to allocate descriptor sets (generic)
  VkDescriptorPoolSize dog_pool_sizes[2];
  dog_pool_sizes[0] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = detector->mem->max_nb_octaves};
  dog_pool_sizes[1] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = detector->mem->max_nb_octaves};
  VkDescriptorPoolCreateInfo dog_descriptor_pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = detector->mem->max_nb_octaves, .poolSizeCount = 2, .pPoolSizes = dog_pool_sizes};
  if (vkCreateDescriptorPool(detector->dev->device, &dog_descriptor_pool_info, NULL, &detector->dog_desc_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create DifferenceOfGaussian descriptor pool");
    return false;
  }

  // Create descriptor sets that can be bound in command buffer
  VkDescriptorSetLayout *dog_layouts = allocMultLayoutCopy(detector->dog_desc_set_layout, detector->mem->max_nb_octaves);
  detector->dog_desc_sets = (VkDescriptorSet *)malloc(sizeof(VkDescriptorSet) * detector->mem->max_nb_octaves);
  VkDescriptorSetAllocateInfo dog_alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                .descriptorPool = detector->dog_desc_pool,
                                                .descriptorSetCount = detector->mem->max_nb_octaves,
                                                .pSetLayouts = dog_layouts};
  alloc_res = vkAllocateDescriptorSets(detector->dev->device, &dog_alloc_info, detector->dog_desc_sets);
  free(dog_layouts);
  if (alloc_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate DifferenceOfGaussian descriptor set");
    return false;
  }

  return true;
}

bool setupComputePipelines(vksift_SiftDetector detector)
{
  //////////////////////////////////////
  // Setup GaussianBlur pipeline
  //////////////////////////////////////
  VkShaderModule blur_shader_module;
  if (detector->use_hardware_interp_kernel)
  {
    if (!vkenv_createShaderModule(detector->dev->device, "shaders/GaussianBlurInterpolated.comp.spv", &blur_shader_module))
    {
      logError(LOG_TAG, "Failed to create GaussianBlurInterpolated shader module");
      return false;
    }
  }
  else if (!vkenv_createShaderModule(detector->dev->device, "shaders/GaussianBlur.comp.spv", &blur_shader_module))
  {
    logError(LOG_TAG, "Failed to create GaussianBlur shader module");
    return false;
  }
  if (!vkenv_createComputePipeline(detector->dev->device, blur_shader_module, detector->blur_desc_set_layout, sizeof(GaussianBlurPushConsts),
                                   &detector->blur_pipeline_layout, &detector->blur_pipeline))
  {
    logError(LOG_TAG, "Failed to create GaussianBlur pipeline");
    vkDestroyShaderModule(detector->dev->device, blur_shader_module, NULL);
    return false;
  }
  vkDestroyShaderModule(detector->dev->device, blur_shader_module, NULL);

  //////////////////////////////////////
  // Setup DifferenceOfGaussian pipeline
  //////////////////////////////////////
  VkShaderModule dog_shader_module;
  if (!vkenv_createShaderModule(detector->dev->device, "shaders/DifferenceOfGaussian.comp.spv", &dog_shader_module))
  {
    logError(LOG_TAG, "Failed to create DifferenceOfGaussian shader module");
    return false;
  }
  if (!vkenv_createComputePipeline(detector->dev->device, dog_shader_module, detector->dog_desc_set_layout, 0, &detector->dog_pipeline_layout,
                                   &detector->dog_pipeline))
  {
    logError(LOG_TAG, "Failed to create DifferenceOfGaussian pipeline");
    vkDestroyShaderModule(detector->dev->device, dog_shader_module, NULL);
    return false;
  }
  vkDestroyShaderModule(detector->dev->device, dog_shader_module, NULL);

  return true;
}

bool setupSyncObjects(vksift_SiftDetector detector)
{
  // TODO: depends on async or not, for async we may need multiple semaphores (maybe one for each octave)
  /*if(detector->dev->async_compute_available) {

  }
  else {

  }*/

  VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
  if (vkCreateFence(detector->dev->device, &fence_create_info, NULL, &detector->end_of_detection_fence) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create a Vulkan fence");
    return false;
  }
  return true;
}

bool writeDescriptorSets(vksift_SiftDetector detector)
{
  /////////////////////////////////////////////////////
  // Write sets for gaussian blur pipeline
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    VkDescriptorImageInfo blur_input_image_info = {
        .sampler = detector->image_sampler, .imageView = detector->mem->octave_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo blur_work_image_info = {
        .sampler = detector->image_sampler, .imageView = detector->mem->blur_tmp_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo blur_output_image_info = {
        .sampler = VK_NULL_HANDLE, .imageView = detector->mem->octave_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet blur_descriptor_writes[2];
    // First write for horizontal pass
    blur_descriptor_writes[0] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                       .dstSet = detector->blur_h_desc_sets[i],
                                                       .dstBinding = 0,
                                                       .dstArrayElement = 0,
                                                       .descriptorCount = 1,
                                                       .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                       .pImageInfo = &blur_input_image_info,
                                                       .pBufferInfo = NULL,
                                                       .pTexelBufferView = NULL};
    blur_descriptor_writes[1] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                       .dstSet = detector->blur_h_desc_sets[i],
                                                       .dstBinding = 1,
                                                       .dstArrayElement = 0,
                                                       .descriptorCount = 1,
                                                       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                       .pImageInfo = &blur_work_image_info,
                                                       .pBufferInfo = NULL,
                                                       .pTexelBufferView = NULL};
    vkUpdateDescriptorSets(detector->dev->device, 2, blur_descriptor_writes, 0, NULL);
    // Then write for vertical pass
    blur_descriptor_writes[0].dstSet = detector->blur_v_desc_sets[i];
    blur_descriptor_writes[0].pImageInfo = &blur_work_image_info;
    blur_descriptor_writes[1].dstSet = detector->blur_v_desc_sets[i];
    blur_descriptor_writes[1].pImageInfo = &blur_output_image_info;
    vkUpdateDescriptorSets(detector->dev->device, 2, blur_descriptor_writes, 0, NULL);
  }

  /////////////////////////////////////////////////////
  // Write sets for Difference of Gaussian pipeline
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    VkDescriptorImageInfo dog_input_image_info = {
        .sampler = VK_NULL_HANDLE, .imageView = detector->mem->octave_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo dog_output_image_info = {
        .sampler = VK_NULL_HANDLE, .imageView = detector->mem->octave_DoG_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet dog_descriptor_writes[2];
    dog_descriptor_writes[0] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                      .dstSet = detector->dog_desc_sets[i],
                                                      .dstBinding = 0,
                                                      .dstArrayElement = 0,
                                                      .descriptorCount = 1,
                                                      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                      .pImageInfo = &dog_input_image_info,
                                                      .pBufferInfo = NULL,
                                                      .pTexelBufferView = NULL};
    dog_descriptor_writes[1] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                      .dstSet = detector->dog_desc_sets[i],
                                                      .dstBinding = 1,
                                                      .dstArrayElement = 0,
                                                      .descriptorCount = 1,
                                                      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                      .pImageInfo = &dog_output_image_info,
                                                      .pBufferInfo = NULL,
                                                      .pTexelBufferView = NULL};
    vkUpdateDescriptorSets(detector->dev->device, 2, dog_descriptor_writes, 0, NULL);
  }

  return true;
}

void recClearDataCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf)
{
  /////////////////////////////////////////////////
  // Clear data
  /////////////////////////////////////////////////
  beginMarkerRegion(detector, cmdbuf, "Clear data");

  vkCmdFillBuffer(cmdbuf, detector->mem->indirect_orientation_dispatch_buffer, 0, VK_WHOLE_SIZE, 1);
  vkCmdFillBuffer(cmdbuf, detector->mem->indirect_descriptor_dispatch_buffer, 0, VK_WHOLE_SIZE, 1);
  uint32_t sift_section_offset = 0;
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    // Only reset the indirect dispatch buffers and the SIFT buffer section headers (sift counter and max nb sift)
    uint32_t section_max_nb_feat = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_max_nb_feat_arr[i];
    vkCmdFillBuffer(cmdbuf, detector->mem->sift_buffer_arr[detector->curr_buffer_idx], sift_section_offset, sizeof(uint32_t), 0);
    vkCmdFillBuffer(cmdbuf, detector->mem->sift_buffer_arr[detector->curr_buffer_idx], sift_section_offset + sizeof(uint32_t), sizeof(uint32_t),
                    section_max_nb_feat);
    sift_section_offset += sizeof(vksift_Feature) * section_max_nb_feat;

    // Set the group size x to 0 for the orientation and descriptor
    vkCmdFillBuffer(cmdbuf, detector->mem->indirect_orientation_dispatch_buffer, sizeof(uint32_t) * 3 * i, sizeof(uint32_t), 0);
    vkCmdFillBuffer(cmdbuf, detector->mem->indirect_descriptor_dispatch_buffer, sizeof(uint32_t) * 3 * i, sizeof(uint32_t), 0);
  }
  endMarkerRegion(detector, cmdbuf);
}
void recCopyInputImageCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf)
{
  /////////////////////////////////////////////////
  // Copy input image
  /////////////////////////////////////////////////
  beginMarkerRegion(detector, cmdbuf, "CopyInputImage");
  // Setup input image layout and access for transfer
  VkImageMemoryBarrier image_barrier;
  image_barrier = vkenv_genImageMemoryBarrier(
      detector->mem->input_image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1});
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &image_barrier);
  // Copy from staging buffer to device only image command
  VkBufferImageCopy buffer_image_region = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
      .imageOffset = {.x = 0, .y = 0, .z = 0},
      .imageExtent = {.width = detector->mem->curr_input_image_width, .height = detector->mem->curr_input_image_height, .depth = 1}};
  vkCmdCopyBufferToImage(cmdbuf, detector->mem->image_staging_buffer, detector->mem->input_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &buffer_image_region);
  // Setup input image layout and access for compute shaders
  image_barrier = vkenv_genImageMemoryBarrier(
      detector->mem->input_image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
      VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
      (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1});
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &image_barrier);

  endMarkerRegion(detector, cmdbuf);
}

void recScaleSpaceConstructionCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_idx)
{
  /////////////////////////////////////////////////
  // Scale space construction
  /////////////////////////////////////////////////
  beginMarkerRegion(detector, cmdbuf, "Scale space construction");
  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline);

  VkImageMemoryBarrier image_barriers[2];
  uint32_t nb_scales = detector->mem->nb_scales_per_octave;
  GaussianBlurPushConsts blur_push_const;

  // Handle the octave first scale
  if (oct_idx == 0)
  {
    // Copy input image (convert to pyr format and upscale if needed) then blur it to get (Octave 0,Scale 0)
    VkImageBlit region = {
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .srcOffsets = {{0, 0, 0}, {(int32_t)detector->mem->curr_input_image_width, (int32_t)detector->mem->curr_input_image_height, 1}},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .dstOffsets = {{0, 0, 0},
                       {(int32_t)detector->mem->octave_resolutions[oct_idx].width, (int32_t)detector->mem->octave_resolutions[oct_idx].height, 1}}};
    vkCmdBlitImage(cmdbuf, detector->mem->input_image, VK_IMAGE_LAYOUT_GENERAL, detector->mem->octave_image_arr[oct_idx], VK_IMAGE_LAYOUT_GENERAL, 1,
                   &region, VK_FILTER_LINEAR);

    // Setup memory access (horizontal pass read from source scale and write to temporary restul image)
    image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->blur_tmp_image_arr[oct_idx], 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    image_barriers[1] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 3});
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);

    // Horizontal blur the first scale
    blur_push_const.is_vertical = 0;
    blur_push_const.array_layer = 0;
    blur_push_const.kernel_size = detector->gaussian_kernel_sizes[0];
    memcpy(blur_push_const.kernel, detector->gaussian_kernels, sizeof(float) * VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE);

    vkCmdPushConstants(cmdbuf, detector->blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &blur_push_const);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline_layout, 0, 1, &detector->blur_h_desc_sets[oct_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), 1);

    // Setup the memory access masks for vertical pass (read from temp result and write to target scale)
    image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->blur_tmp_image_arr[oct_idx], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    image_barriers[1] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 3});
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);

    // Vertical blur the first scale
    blur_push_const.is_vertical = 1;
    vkCmdPushConstants(cmdbuf, detector->blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &blur_push_const);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline_layout, 0, 1, &detector->blur_v_desc_sets[oct_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), 1);
  }
  else
  {
    // If this is not the first octave, downscale the scale (Octave i-1,Scale nb_scale_per_octave) to get (Octave i,Scale 0)
    VkImageBlit region = {
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = nb_scales, .layerCount = 1},
        .srcOffsets = {{0, 0, 0},
                       {(int32_t)detector->mem->octave_resolutions[oct_idx - 1].width, (int32_t)detector->mem->octave_resolutions[oct_idx - 1].height, 1}},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .dstOffsets = {{0, 0, 0},
                       {(int32_t)detector->mem->octave_resolutions[oct_idx].width, (int32_t)detector->mem->octave_resolutions[oct_idx].height, 1}}};
    vkCmdBlitImage(cmdbuf, detector->mem->octave_image_arr[oct_idx - 1], VK_IMAGE_LAYOUT_GENERAL, detector->mem->octave_image_arr[oct_idx],
                   VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_NEAREST);
  }

  for (uint32_t scale_i = 1; scale_i < (nb_scales + 3); scale_i++)
  {
    // Gaussian blur from one scale to the next
    // Setup read/write access for relevant scales
    image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->blur_tmp_image_arr[oct_idx], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    image_barriers[1] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 3});
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);

    // Horizontal blur
    blur_push_const.is_vertical = 0;
    blur_push_const.array_layer = scale_i - 1;
    blur_push_const.kernel_size = detector->gaussian_kernel_sizes[scale_i];
    memcpy(blur_push_const.kernel, &detector->gaussian_kernels[scale_i * VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE],
           sizeof(float) * VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE);

    vkCmdPushConstants(cmdbuf, detector->blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &blur_push_const);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline_layout, 0, 1, &detector->blur_h_desc_sets[oct_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), 1);
    // Change read/write acces for vertical pass
    image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->blur_tmp_image_arr[oct_idx], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    image_barriers[1] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 3});
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);

    // Vertical blur
    blur_push_const.is_vertical = 1;
    blur_push_const.array_layer = scale_i;

    vkCmdPushConstants(cmdbuf, detector->blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &blur_push_const);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline_layout, 0, 1, &detector->blur_v_desc_sets[oct_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), 1);
  }
  // Make sure octave image writes are available for compute after leaving this function
  image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                  (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 3});
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, image_barriers);

  endMarkerRegion(detector, cmdbuf);
}

void recDifferenceOfGaussianCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_begin, const uint32_t oct_count)
{
  VkImageMemoryBarrier *image_barriers = (VkImageMemoryBarrier *)malloc(sizeof(VkImageMemoryBarrier) * oct_count);
  uint32_t nb_scales = detector->mem->nb_scales_per_octave;

  /////////////////////////////////////////////////
  // DifferenceOfGaussian
  /////////////////////////////////////////////////
  beginMarkerRegion(detector, cmdbuf, "DoG computation");

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->dog_pipeline);
  // Make sure the DoG images can be written into
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    image_barriers[oct_idx] = vkenv_genImageMemoryBarrier(
        detector->mem->octave_DoG_image_arr[oct_idx], 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 2});
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, oct_count, image_barriers);

  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->dog_pipeline_layout, 0, 1, &detector->dog_desc_sets[oct_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), nb_scales + 2);
  }

  // Make the DoG images data readable for compute ops after this function
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    image_barriers[oct_idx] =
        vkenv_genImageMemoryBarrier(detector->mem->octave_DoG_image_arr[oct_idx], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 2});
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, oct_count, image_barriers);

  endMarkerRegion(detector, cmdbuf);

  free(image_barriers);
}

bool recordCommandBuffers(vksift_SiftDetector detector)
{
  VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0, .pInheritanceInfo = NULL};

  if (vkBeginCommandBuffer(detector->sync_command_buffer, &begin_info) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to begin the command buffer recording");
    return false;
  }

  // Clear data
  recClearDataCmds(detector, detector->sync_command_buffer);
  // Copy input image
  recCopyInputImageCmds(detector, detector->sync_command_buffer);
  // Scale space construction
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    // Construct each octave
    recScaleSpaceConstructionCmds(detector, detector->sync_command_buffer, i);
  }
  // Compute difference of Gaussian (on full range to synchronize every octave with a single barrier)
  recDifferenceOfGaussianCmds(detector, detector->sync_command_buffer, 0, detector->mem->curr_nb_octaves);

  // TODO: same range system for extract, orientation, descriptor and copy count

  /////////////////////////////////////////////////
  // Scale space construction
  /////////////////////////////////////////////////
  beginMarkerRegion(detector, detector->sync_command_buffer, "Scale space construction");
  endMarkerRegion(detector, detector->sync_command_buffer);

  if (vkEndCommandBuffer(detector->sync_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to record command buffer");
    return false;
  }

  return true;
}

bool vksift_createSiftDetector(vkenv_Device device, vksift_SiftMemory memory, vksift_SiftDetector *detector_ptr, const vksift_Config *config)
{
  assert(device != NULL);
  assert(memory != NULL);
  assert(detector_ptr != NULL);
  assert(*detector_ptr == NULL);
  assert(config != NULL);
  *detector_ptr = (vksift_SiftDetector)malloc(sizeof(struct vksift_SiftDetector_T));
  vksift_SiftDetector detector = *detector_ptr;
  memset(detector, 0, sizeof(struct vksift_SiftDetector_T));

  // Store parent device and memory
  detector->dev = device;
  detector->mem = memory;
  // Retrieve config
  detector->use_hardware_interp_kernel = config->use_hardware_interpolated_blur;
  detector->input_blur_level = config->input_image_blur_level;
  detector->seed_scale_sigma = config->seed_scale_sigma;

  detector->curr_buffer_idx = 0u; // Default target buffer is 0 (always available)

  // Try to find GPU debug marker functions
  getGPUDebugMarkerFuncs(detector);
  // Compute the Gaussian kernels used to build the scalespaces
  setupGaussianKernels(detector);

  if (setupCommandPools(detector) && allocateCommandBuffers(detector) && setupImageSampler(detector) && prepareDescriptorSets(detector) &&
      setupComputePipelines(detector) && setupSyncObjects(detector) && writeDescriptorSets(detector) && recordCommandBuffers(detector))
  {
    return true;
  }
  else
  {
    logError(LOG_TAG, "Failed to setup the SiftDetector instance");
    return false;
  }
}

bool vksift_dispatchSiftDetector(vksift_SiftDetector detector, const uint32_t target_buffer_idx, const bool memory_layout_updated)
{
  // We need to setup the descriptor sets and command buffers if the input resolution or target buffer changed
  if (memory_layout_updated || detector->curr_buffer_idx != target_buffer_idx)
  {
    detector->curr_buffer_idx = target_buffer_idx;
    writeDescriptorSets(detector);
    recordCommandBuffers(detector);
    logError(LOG_TAG, "rewrite stuff");
  }

  vkResetFences(detector->dev->device, 1, &detector->end_of_detection_fence);
  {
    VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .waitSemaphoreCount = 0,
                                .commandBufferCount = 1,
                                .pCommandBuffers = &detector->sync_command_buffer,
                                .signalSemaphoreCount = 0};
    if (vkQueueSubmit(detector->dev->general_queue, 1, &submit_info, detector->end_of_detection_fence) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to submit command buffer");
      return false;
    }
  }
  return true;
}

void vksift_destroySiftDetector(vksift_SiftDetector *detector_ptr)
{
  assert(detector_ptr != NULL);
  assert(*detector_ptr != NULL); // vksift_destroySiftDetector shouldn't be called on NULL vksift_SiftMemory object
  vksift_SiftDetector detector = *detector_ptr;

  // Destroy sync objects
  VK_NULL_SAFE_DELETE(detector->end_of_detection_fence, vkDestroyFence(detector->dev->device, detector->end_of_detection_fence, NULL));

  // Destroy sampler
  VK_NULL_SAFE_DELETE(detector->image_sampler, vkDestroySampler(detector->dev->device, detector->image_sampler, NULL));

  // Destroy command pools
  VK_NULL_SAFE_DELETE(detector->general_command_pool, vkDestroyCommandPool(detector->dev->device, detector->general_command_pool, NULL));
  if (detector->dev->async_compute_available)
  {
    VK_NULL_SAFE_DELETE(detector->async_compute_command_pool, vkDestroyCommandPool(detector->dev->device, detector->async_compute_command_pool, NULL));
  }

  // Destroy pipelines and descriptors
  // Gaussian blur
  VK_NULL_SAFE_DELETE(detector->blur_pipeline, vkDestroyPipeline(detector->dev->device, detector->blur_pipeline, NULL));
  VK_NULL_SAFE_DELETE(detector->blur_pipeline_layout, vkDestroyPipelineLayout(detector->dev->device, detector->blur_pipeline_layout, NULL));
  VK_NULL_SAFE_DELETE(detector->blur_desc_pool, vkDestroyDescriptorPool(detector->dev->device, detector->blur_desc_pool, NULL));
  VK_NULL_SAFE_DELETE(detector->blur_desc_set_layout, vkDestroyDescriptorSetLayout(detector->dev->device, detector->blur_desc_set_layout, NULL));
  // Difference of Gaussian
  VK_NULL_SAFE_DELETE(detector->dog_pipeline, vkDestroyPipeline(detector->dev->device, detector->dog_pipeline, NULL));
  VK_NULL_SAFE_DELETE(detector->dog_pipeline_layout, vkDestroyPipelineLayout(detector->dev->device, detector->dog_pipeline_layout, NULL));
  VK_NULL_SAFE_DELETE(detector->dog_desc_pool, vkDestroyDescriptorPool(detector->dev->device, detector->dog_desc_pool, NULL));
  VK_NULL_SAFE_DELETE(detector->dog_desc_set_layout, vkDestroyDescriptorSetLayout(detector->dev->device, detector->dog_desc_set_layout, NULL));

  // Free descriptor arrays
  VK_NULL_SAFE_DELETE(detector->gaussian_kernels, free(detector->gaussian_kernels));
  VK_NULL_SAFE_DELETE(detector->gaussian_kernel_sizes, free(detector->gaussian_kernel_sizes));
  VK_NULL_SAFE_DELETE(detector->blur_desc_sets, free(detector->blur_desc_sets));
  VK_NULL_SAFE_DELETE(detector->dog_desc_sets, free(detector->dog_desc_sets));

  // Release vksift_SiftDetector memory
  free(*detector_ptr);
  *detector_ptr = NULL;
}