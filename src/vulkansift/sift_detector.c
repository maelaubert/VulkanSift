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

typedef struct
{
  int32_t octave_idx;
  float seed_scale_sigma;
  float dog_threshold;
  float edge_threshold;
} ExtractKeypointsPushConsts;

static void getGPUDebugMarkerFuncs(vksift_SiftDetector detector)
{
  detector->vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(detector->dev->device, "vkCmdDebugMarkerBeginEXT");
  detector->vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(detector->dev->device, "vkCmdDebugMarkerEndEXT");
  detector->debug_marker_supported = (detector->vkCmdDebugMarkerBeginEXT != NULL) && (detector->vkCmdDebugMarkerEndEXT != NULL);
}

static void beginMarkerRegion(vksift_SiftDetector detector, VkCommandBuffer cmd_buf, const char *region_name)
{
  if (detector->debug_marker_supported)
  {
    VkDebugMarkerMarkerInfoEXT marker_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, .pMarkerName = region_name};
    detector->vkCmdDebugMarkerBeginEXT(cmd_buf, &marker_info);
  }
}
static void endMarkerRegion(vksift_SiftDetector detector, VkCommandBuffer cmd_buf)
{
  if (detector->debug_marker_supported)
  {
    detector->vkCmdDebugMarkerEndEXT(cmd_buf);
  }
}

static void setupGaussianKernels(vksift_SiftDetector detector)
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

    logDebug(LOG_TAG, "Gaussian kernels");
    logDebug(LOG_TAG, "Scale %d sigma=%f kernel size=%d", scale_i, sep_kernel_sigma, kernel_size);
    for (uint32_t i = 0; i < kernel_size; i++)
    {
      kernel_tmp_data[i] /= sum_kernel;
      logDebug(LOG_TAG, "%f", kernel_tmp_data[i]);
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
  }
}

static bool setupCommandPools(vksift_SiftDetector detector)
{
  VkCommandPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                       .queueFamilyIndex = detector->dev->general_queues_family_idx};
  if (vkCreateCommandPool(detector->dev->device, &pool_info, NULL, &detector->general_command_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create the general-purpose command pool");
    return false;
  }

  if (detector->dev->async_transfer_available)
  {
    VkCommandPoolCreateInfo async_pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                               .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                               .queueFamilyIndex = detector->dev->async_transfer_queues_family_idx};
    if (vkCreateCommandPool(detector->dev->device, &async_pool_info, NULL, &detector->async_transfer_command_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create the asynchronous transfer command pool");
      return false;
    }
  }

  return true;
}

static bool allocateCommandBuffers(vksift_SiftDetector detector)
{
  VkCommandBufferAllocateInfo allocate_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                               .pNext = NULL,
                                               .commandPool = detector->general_command_pool,
                                               .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                               .commandBufferCount = 1};
  if (vkAllocateCommandBuffers(detector->dev->device, &allocate_info, &detector->detection_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate the detection command buffe");
    return false;
  }
  if (vkAllocateCommandBuffers(detector->dev->device, &allocate_info, &detector->end_of_detection_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate the end-of-detection command buffer");
    return false;
  }

  // If the async tranfer queue is available the SIFT buffers are owned by the transfer queue family
  // in this case we need to release this ownership from the transfer queue before using the buffers in the general purpose queue
  if (detector->dev->async_transfer_available)
  {
    allocate_info.commandPool = detector->async_transfer_command_pool;
    if (vkAllocateCommandBuffers(detector->dev->device, &allocate_info, &detector->release_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate the release-buffer-ownership command buffer on the async transfer pool");
      return false;
    }
    if (vkAllocateCommandBuffers(detector->dev->device, &allocate_info, &detector->acquire_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate the acquire-buffer-ownership command buffer on the async transfer pool");
      return false;
    }
  }

  return true;
}

static bool setupImageSampler(vksift_SiftDetector detector)
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

static VkDescriptorSetLayout *allocMultLayoutCopy(VkDescriptorSetLayout layout, int nb_copy)
{
  VkDescriptorSetLayout *layout_arr = (VkDescriptorSetLayout *)malloc(sizeof(VkDescriptorSetLayout) * nb_copy);
  for (int i = 0; i < nb_copy; i++)
  {
    layout_arr[i] = layout;
  }
  return layout_arr;
}

static bool prepareDescriptorSets(vksift_SiftDetector detector)
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

  ///////////////////////////////////////////////////
  // Descriptors for ExtractKeypoints pipeline
  ///////////////////////////////////////////////////
  VkDescriptorSetLayoutBinding extkpts_dog_image_layout_binding = {.binding = 0,
                                                                   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                                   .descriptorCount = 1,
                                                                   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                   .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding extkpts_sift_buffer_layout_binding = {.binding = 1,
                                                                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                     .descriptorCount = 1,
                                                                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                     .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding extkpts_indispatch_buffer_layout_binding = {.binding = 2,
                                                                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                           .descriptorCount = 1,
                                                                           .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                           .pImmutableSamplers = NULL};

  VkDescriptorSetLayoutBinding extkpts_bindings[3] = {extkpts_dog_image_layout_binding, extkpts_sift_buffer_layout_binding,
                                                      extkpts_indispatch_buffer_layout_binding};

  VkDescriptorSetLayoutCreateInfo extkpts_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 3, .pBindings = extkpts_bindings};

  if (vkCreateDescriptorSetLayout(detector->dev->device, &extkpts_layout_info, NULL, &detector->extractkpts_desc_set_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create ExtractKeypoints descriptor set layout");
    return false;
  }

  // Create descriptor pool to allocate descriptor sets (generic)
  VkDescriptorPoolSize extkpts_pool_sizes[3];
  extkpts_pool_sizes[0] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = detector->mem->max_nb_octaves};
  extkpts_pool_sizes[1] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = detector->mem->max_nb_octaves};
  extkpts_pool_sizes[2] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = detector->mem->max_nb_octaves};
  VkDescriptorPoolCreateInfo extkpts_descriptor_pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                             .maxSets = detector->mem->max_nb_octaves,
                                                             .poolSizeCount = 3,
                                                             .pPoolSizes = extkpts_pool_sizes};
  if (vkCreateDescriptorPool(detector->dev->device, &extkpts_descriptor_pool_info, NULL, &detector->extractkpts_desc_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create ExtractKeypoints descriptor pool");
    return false;
  }

  // Create descriptor sets that can be bound in command buffer
  VkDescriptorSetLayout *extkpts_layouts = allocMultLayoutCopy(detector->extractkpts_desc_set_layout, detector->mem->max_nb_octaves);
  detector->extractkpts_desc_sets = (VkDescriptorSet *)malloc(sizeof(VkDescriptorSet) * detector->mem->max_nb_octaves);
  VkDescriptorSetAllocateInfo extkpts_alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                    .descriptorPool = detector->extractkpts_desc_pool,
                                                    .descriptorSetCount = detector->mem->max_nb_octaves,
                                                    .pSetLayouts = extkpts_layouts};

  alloc_res = vkAllocateDescriptorSets(detector->dev->device, &extkpts_alloc_info, detector->extractkpts_desc_sets);
  free(extkpts_layouts);
  if (alloc_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate ExtractKeypoints descriptor set");
    return false;
  }

  ///////////////////////////////////////////////////
  // Descriptors for ComputeOrientation pipeline
  ///////////////////////////////////////////////////
  VkDescriptorSetLayoutBinding orientation_octave_image_layout_binding = {.binding = 0,
                                                                          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                                          .descriptorCount = 1,
                                                                          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                          .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding orientation_sift_buffer_layout_binding = {.binding = 1,
                                                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                         .descriptorCount = 1,
                                                                         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                         .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding orientation_indispatch_buffer_layout_binding = {.binding = 2,
                                                                               .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                               .descriptorCount = 1,
                                                                               .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                               .pImmutableSamplers = NULL};

  VkDescriptorSetLayoutBinding orientation_bindings[3] = {orientation_octave_image_layout_binding, orientation_sift_buffer_layout_binding,
                                                          orientation_indispatch_buffer_layout_binding};
  VkDescriptorSetLayoutCreateInfo orientation_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 3, .pBindings = orientation_bindings};

  if (vkCreateDescriptorSetLayout(detector->dev->device, &orientation_layout_info, NULL, &detector->orientation_desc_set_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create ComputeOrientation descriptor set layout");
    return false;
  }

  // Create descriptor pool to allocate descriptor sets (generic)
  VkDescriptorPoolSize orientation_pool_sizes[3];
  orientation_pool_sizes[0] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = detector->mem->max_nb_octaves};
  orientation_pool_sizes[1] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = detector->mem->max_nb_octaves};
  orientation_pool_sizes[2] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = detector->mem->max_nb_octaves};
  VkDescriptorPoolCreateInfo orientation_descriptor_pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                                 .maxSets = detector->mem->max_nb_octaves,
                                                                 .poolSizeCount = 3,
                                                                 .pPoolSizes = orientation_pool_sizes};
  if (vkCreateDescriptorPool(detector->dev->device, &orientation_descriptor_pool_info, NULL, &detector->orientation_desc_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create ComputeOrientation descriptor pool");
    return false;
  }

  // Create descriptor sets that can be bound in command buffer
  VkDescriptorSetLayout *orientation_layouts = allocMultLayoutCopy(detector->orientation_desc_set_layout, detector->mem->max_nb_octaves);
  detector->orientation_desc_sets = (VkDescriptorSet *)malloc(sizeof(VkDescriptorSet) * detector->mem->max_nb_octaves);
  VkDescriptorSetAllocateInfo orientation_alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                        .descriptorPool = detector->orientation_desc_pool,
                                                        .descriptorSetCount = detector->mem->max_nb_octaves,
                                                        .pSetLayouts = orientation_layouts};

  alloc_res = vkAllocateDescriptorSets(detector->dev->device, &orientation_alloc_info, detector->orientation_desc_sets);
  free(orientation_layouts);
  if (alloc_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate ComputeOrientation descriptor set");
    return false;
  }

  ///////////////////////////////////////////////////
  // Descriptors for ComputeDescriptors pipeline
  ///////////////////////////////////////////////////

  VkDescriptorSetLayoutBinding descriptor_octave_image_layout_binding = {.binding = 0,
                                                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                                         .descriptorCount = 1,
                                                                         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                         .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding descriptor_sift_buffer_layout_binding = {.binding = 1,
                                                                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                        .descriptorCount = 1,
                                                                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                        .pImmutableSamplers = NULL};

  VkDescriptorSetLayoutBinding descriptor_bindings[2] = {descriptor_octave_image_layout_binding, descriptor_sift_buffer_layout_binding};

  VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = descriptor_bindings};

  if (vkCreateDescriptorSetLayout(detector->dev->device, &descriptor_layout_info, NULL, &detector->descriptor_desc_set_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create ComputeDescriptors descriptor set layout");
    return false;
  }

  // Create descriptor pool to allocate descriptor sets (generic)
  VkDescriptorPoolSize descriptor_pool_sizes[2];
  descriptor_pool_sizes[0] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = detector->mem->max_nb_octaves};
  descriptor_pool_sizes[1] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = detector->mem->max_nb_octaves};
  VkDescriptorPoolCreateInfo descriptor_descriptor_pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                                .maxSets = detector->mem->max_nb_octaves,
                                                                .poolSizeCount = 2,
                                                                .pPoolSizes = descriptor_pool_sizes};
  if (vkCreateDescriptorPool(detector->dev->device, &descriptor_descriptor_pool_info, NULL, &detector->descriptor_desc_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create ComputeDescriptors descriptor pool");
    return false;
  }

  // Create descriptor sets that can be bound in command buffer
  VkDescriptorSetLayout *descriptor_layouts = allocMultLayoutCopy(detector->descriptor_desc_set_layout, detector->mem->max_nb_octaves);
  detector->descriptor_desc_sets = (VkDescriptorSet *)malloc(sizeof(VkDescriptorSet) * detector->mem->max_nb_octaves);
  VkDescriptorSetAllocateInfo descriptor_alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                       .descriptorPool = detector->descriptor_desc_pool,
                                                       .descriptorSetCount = detector->mem->max_nb_octaves,
                                                       .pSetLayouts = descriptor_layouts};

  alloc_res = vkAllocateDescriptorSets(detector->dev->device, &descriptor_alloc_info, detector->descriptor_desc_sets);
  free(descriptor_layouts);
  if (alloc_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate ComputeDescriptors descriptor set");
    return false;
  }

  return true;
}

static bool setupComputePipelines(vksift_SiftDetector detector)
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

  //////////////////////////////////////
  // Setup ExtractKeypoints pipeline
  //////////////////////////////////////
  VkShaderModule extractkpts_shader_module;
  if (!vkenv_createShaderModule(detector->dev->device, "shaders/ExtractKeypoints.comp.spv", &extractkpts_shader_module))
  {
    logError(LOG_TAG, "Failed to create ExtractKeypoints shader module");
    return false;
  }
  if (!vkenv_createComputePipeline(detector->dev->device, extractkpts_shader_module, detector->extractkpts_desc_set_layout,
                                   sizeof(ExtractKeypointsPushConsts), &detector->extractkpts_pipeline_layout, &detector->extractkpts_pipeline))
  {
    logError(LOG_TAG, "Failed to create ExtractKeypoints pipeline");
    vkDestroyShaderModule(detector->dev->device, extractkpts_shader_module, NULL);
    return false;
  }
  vkDestroyShaderModule(detector->dev->device, extractkpts_shader_module, NULL);
  //////////////////////////////////////
  // Setup ComputeOrientation pipeline
  //////////////////////////////////////

  VkShaderModule orientation_shader_module;
  if (!vkenv_createShaderModule(detector->dev->device, "shaders/ComputeOrientation.comp.spv", &orientation_shader_module))
  {
    logError(LOG_TAG, "Failed to create ComputeOrientation shader module");
    return false;
  }
  if (!vkenv_createComputePipeline(detector->dev->device, orientation_shader_module, detector->orientation_desc_set_layout, 0,
                                   &detector->orientation_pipeline_layout, &detector->orientation_pipeline))
  {
    logError(LOG_TAG, "Failed to create ComputeOrientation pipeline");
    vkDestroyShaderModule(detector->dev->device, orientation_shader_module, NULL);
    return false;
  }
  vkDestroyShaderModule(detector->dev->device, orientation_shader_module, NULL);

  //////////////////////////////////////
  // Setup ComputeDescriptors pipeline
  //////////////////////////////////////

  VkShaderModule descriptor_shader_module;
  if (!vkenv_createShaderModule(detector->dev->device, "shaders/ComputeDescriptors.comp.spv", &descriptor_shader_module))
  {
    logError(LOG_TAG, "Failed to create ComputeDescriptors shader module");
    return false;
  }
  if (!vkenv_createComputePipeline(detector->dev->device, descriptor_shader_module, detector->descriptor_desc_set_layout, 0,
                                   &detector->descriptor_pipeline_layout, &detector->descriptor_pipeline))
  {
    logError(LOG_TAG, "Failed to create ComputeDescriptors pipeline");
    vkDestroyShaderModule(detector->dev->device, descriptor_shader_module, NULL);
    return false;
  }
  vkDestroyShaderModule(detector->dev->device, descriptor_shader_module, NULL);

  return true;
}

static bool setupSyncObjects(vksift_SiftDetector detector)
{
  VkSemaphoreCreateInfo semaphore_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = NULL, .flags = 0};
  if (vkCreateSemaphore(detector->dev->device, &semaphore_create_info, NULL, &detector->end_of_detection_semaphore) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create end-of-detection Vulkan semaphore");
    return false;
  }

  if (detector->dev->async_transfer_available)
  {
    if (vkCreateSemaphore(detector->dev->device, &semaphore_create_info, NULL, &detector->buffer_ownership_released_by_transfer_semaphore) != VK_SUCCESS ||
        vkCreateSemaphore(detector->dev->device, &semaphore_create_info, NULL, &detector->buffer_ownership_acquired_by_transfer_semaphore) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create Vulkan semaphores for async transfer queue buffer ownership transfers");
      return false;
    }
  }

  VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
  if (vkCreateFence(detector->dev->device, &fence_create_info, NULL, &detector->end_of_detection_fence) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create a Vulkan fence");
    return false;
  }
  return true;
}

static bool writeDescriptorSets(vksift_SiftDetector detector)
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

  /////////////////////////////////////////////////////
  // Write sets for ExtractKeypoints pipeline
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    VkDescriptorImageInfo dog_input_image_info = {
        .sampler = VK_NULL_HANDLE, .imageView = detector->mem->octave_DoG_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorBufferInfo sift_buffer_info = {.buffer = detector->mem->sift_buffer_arr[detector->curr_buffer_idx],
                                               .offset = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[i],
                                               .range = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[i]};
    VkDescriptorBufferInfo indispatch_buffer_info = {.buffer = detector->mem->indirect_orientation_dispatch_buffer,
                                                     .offset = detector->mem->indirect_oridesc_offset_arr[i],
                                                     .range = sizeof(uint32_t) * 3};
    VkWriteDescriptorSet descriptor_writes[3];
    descriptor_writes[0] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->extractkpts_desc_sets[i],
                                                  .dstBinding = 0,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                  .pImageInfo = &dog_input_image_info,
                                                  .pBufferInfo = NULL,
                                                  .pTexelBufferView = NULL};
    descriptor_writes[1] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->extractkpts_desc_sets[i],
                                                  .dstBinding = 1,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  .pImageInfo = NULL,
                                                  .pBufferInfo = &sift_buffer_info,
                                                  .pTexelBufferView = NULL};
    descriptor_writes[2] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->extractkpts_desc_sets[i],
                                                  .dstBinding = 2,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  .pImageInfo = NULL,
                                                  .pBufferInfo = &indispatch_buffer_info,
                                                  .pTexelBufferView = NULL};
    vkUpdateDescriptorSets(detector->dev->device, 3, descriptor_writes, 0, NULL);
  }

  /////////////////////////////////////////////////////
  // Write sets for ComputeOrientation pipeline
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    VkDescriptorImageInfo octave_input_image_info = {
        .sampler = VK_NULL_HANDLE, .imageView = detector->mem->octave_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorBufferInfo sift_buffer_info = {.buffer = detector->mem->sift_buffer_arr[detector->curr_buffer_idx],
                                               .offset = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[i],
                                               .range = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[i]};
    VkDescriptorBufferInfo indispatch_buffer_info = {.buffer = detector->mem->indirect_descriptor_dispatch_buffer,
                                                     .offset = detector->mem->indirect_oridesc_offset_arr[i],
                                                     .range = sizeof(uint32_t) * 3};

    VkWriteDescriptorSet descriptor_writes[3];
    descriptor_writes[0] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->orientation_desc_sets[i],
                                                  .dstBinding = 0,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                  .pImageInfo = &octave_input_image_info,
                                                  .pBufferInfo = NULL,
                                                  .pTexelBufferView = NULL};
    descriptor_writes[1] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->orientation_desc_sets[i],
                                                  .dstBinding = 1,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  .pImageInfo = NULL,
                                                  .pBufferInfo = &sift_buffer_info,
                                                  .pTexelBufferView = NULL};
    descriptor_writes[2] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->orientation_desc_sets[i],
                                                  .dstBinding = 2,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  .pImageInfo = NULL,
                                                  .pBufferInfo = &indispatch_buffer_info,
                                                  .pTexelBufferView = NULL};
    vkUpdateDescriptorSets(detector->dev->device, 3, descriptor_writes, 0, NULL);
  }

  /////////////////////////////////////////////////////
  // Write sets for ComputeDescriptor pipeline
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    VkDescriptorImageInfo octave_input_image_info = {
        .sampler = VK_NULL_HANDLE, .imageView = detector->mem->octave_image_view_arr[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorBufferInfo sift_buffer_info = {.buffer = detector->mem->sift_buffer_arr[detector->curr_buffer_idx],
                                               .offset = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[i],
                                               .range = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[i]};
    VkWriteDescriptorSet descriptor_writes[2];
    descriptor_writes[0] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->descriptor_desc_sets[i],
                                                  .dstBinding = 0,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                  .pImageInfo = &octave_input_image_info,
                                                  .pBufferInfo = NULL,
                                                  .pTexelBufferView = NULL};
    descriptor_writes[1] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                  .dstSet = detector->descriptor_desc_sets[i],
                                                  .dstBinding = 1,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  .pImageInfo = NULL,
                                                  .pBufferInfo = &sift_buffer_info,
                                                  .pTexelBufferView = NULL};
    vkUpdateDescriptorSets(detector->dev->device, 2, descriptor_writes, 0, NULL);
  }

  return true;
}

static void recCopyInputImageCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf)
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

static void recScaleSpaceConstructionCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_idx)
{
  /////////////////////////////////////////////////
  // Scale space construction
  /////////////////////////////////////////////////
  beginMarkerRegion(detector, cmdbuf, "Scale space construction");
  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline);

  VkImageMemoryBarrier image_barriers[2];
  uint32_t nb_scales = detector->mem->nb_scales_per_octave;
  GaussianBlurPushConsts blur_push_const;

  // Handle the octave first scale (blit from input image)
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
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}); // only scale 0
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
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}); // only scale 0
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);

    // Vertical blur the first scale
    blur_push_const.is_vertical = 1;
    vkCmdPushConstants(cmdbuf, detector->blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &blur_push_const);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline_layout, 0, 1, &detector->blur_v_desc_sets[oct_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), 1);
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
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, scale_i - 1, 1}); // ony prev scale
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
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, scale_i, 1}); // ony curr scale
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);

    // Vertical blur
    blur_push_const.is_vertical = 1;
    blur_push_const.array_layer = scale_i;

    vkCmdPushConstants(cmdbuf, detector->blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &blur_push_const);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->blur_pipeline_layout, 0, 1, &detector->blur_v_desc_sets[oct_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), 1);

    // Make sure the scale image writes are available for compute
    image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, scale_i, 1}); // ony curr scale
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, image_barriers);
  }

  if (oct_idx != (detector->mem->curr_nb_octaves - 1))
  {
    // If this is not the first octave, downscale the scale (Octave i-1,Scale nb_scale_per_octave) to get (Octave i,Scale 0)

    image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, nb_scales, 1});
    image_barriers[1] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx + 1], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);

    VkImageBlit region = {
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = nb_scales, .layerCount = 1},
        .srcOffsets = {{0, 0, 0},
                       {(int32_t)detector->mem->octave_resolutions[oct_idx].width, (int32_t)detector->mem->octave_resolutions[oct_idx].height, 1}},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .dstOffsets = {
            {0, 0, 0},
            {(int32_t)detector->mem->octave_resolutions[oct_idx + 1].width, (int32_t)detector->mem->octave_resolutions[oct_idx + 1].height, 1}}};
    vkCmdBlitImage(cmdbuf, detector->mem->octave_image_arr[oct_idx], VK_IMAGE_LAYOUT_GENERAL, detector->mem->octave_image_arr[oct_idx + 1],
                   VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_NEAREST);

    // Make sure the transfer if done
    image_barriers[0] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx], VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, nb_scales, 1});
    image_barriers[1] = vkenv_genImageMemoryBarrier(detector->mem->octave_image_arr[oct_idx + 1], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, image_barriers);
  }

  endMarkerRegion(detector, cmdbuf);
}

static void recDifferenceOfGaussianCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_begin, const uint32_t oct_count)
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
    image_barriers[oct_idx - oct_begin] = vkenv_genImageMemoryBarrier(
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
    image_barriers[oct_idx - oct_begin] =
        vkenv_genImageMemoryBarrier(detector->mem->octave_DoG_image_arr[oct_idx], VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                    (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, nb_scales + 2});
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, oct_count, image_barriers);

  endMarkerRegion(detector, cmdbuf);

  free(image_barriers);
}

static void recClearBufferDataCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_begin, const uint32_t oct_count)
{
  beginMarkerRegion(detector, cmdbuf, "Clear buffer data");

  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    vkCmdFillBuffer(cmdbuf, detector->mem->indirect_orientation_dispatch_buffer, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3,
                    1);
    vkCmdFillBuffer(cmdbuf, detector->mem->indirect_descriptor_dispatch_buffer, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3,
                    1);
    // Only reset the indirect dispatch buffers and the SIFT buffer section headers (sift counter and max nb sift)
    uint32_t sift_section_offset = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[oct_idx];
    uint32_t section_max_nb_feat = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_max_nb_feat_arr[oct_idx];
    vkCmdFillBuffer(cmdbuf, detector->mem->sift_buffer_arr[detector->curr_buffer_idx], sift_section_offset, sizeof(uint32_t), 0);
    vkCmdFillBuffer(cmdbuf, detector->mem->sift_buffer_arr[detector->curr_buffer_idx], sift_section_offset + sizeof(uint32_t), sizeof(uint32_t),
                    section_max_nb_feat);

    // Set the group size x to 0 for the orientation and descriptor
    vkCmdFillBuffer(cmdbuf, detector->mem->indirect_orientation_dispatch_buffer, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t), 0);
    vkCmdFillBuffer(cmdbuf, detector->mem->indirect_descriptor_dispatch_buffer, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t), 0);
  }

  endMarkerRegion(detector, cmdbuf);
}

static void recExtractKeypointsCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, uint32_t oct_begin, uint32_t oct_count)
{
  /////////////////////////////////////////////////
  // Extract keypoints
  /////////////////////////////////////////////////
  VkBufferMemoryBarrier *buffer_barriers = (VkBufferMemoryBarrier *)malloc(sizeof(VkBufferMemoryBarrier) * oct_count * 2);
  VkBuffer sift_buffer = detector->mem->sift_buffer_arr[detector->curr_buffer_idx];

  beginMarkerRegion(detector, cmdbuf, "ExtractKeypoints");

  // Make sure previous writes are visible
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[(oct_idx - oct_begin) * 2 + 0] =
        vkenv_genBufferMemoryBarrier(sift_buffer, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[oct_idx],
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[oct_idx]);
    buffer_barriers[(oct_idx - oct_begin) * 2 + 1] =
        vkenv_genBufferMemoryBarrier(detector->mem->indirect_orientation_dispatch_buffer, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED,
                                     VK_QUEUE_FAMILY_IGNORED, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, oct_count * 2, buffer_barriers, 0,
                       NULL);

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->extractkpts_pipeline);
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    ExtractKeypointsPushConsts pushconst;
    pushconst.octave_idx = (int32_t)(oct_idx) - (detector->mem->use_upsampling ? 1 : 0);
    pushconst.seed_scale_sigma = detector->seed_scale_sigma;
    pushconst.dog_threshold = detector->intensity_threshold / detector->mem->nb_scales_per_octave;
    pushconst.edge_threshold = detector->edge_threshold;
    // logError(LOG_TAG, "sigmul %f", pushconst.sigma_multiplier);
    vkCmdPushConstants(cmdbuf, detector->extractkpts_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ExtractKeypointsPushConsts), &pushconst);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->extractkpts_pipeline_layout, 0, 1, &detector->extractkpts_desc_sets[oct_idx],
                            0, NULL);
    vkCmdDispatch(cmdbuf, ceilf((float)(detector->mem->octave_resolutions[oct_idx].width) / 8.f),
                  ceilf((float)(detector->mem->octave_resolutions[oct_idx].height) / 8.f), detector->mem->nb_scales_per_octave);
  }

  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[oct_idx - oct_begin] =
        vkenv_genBufferMemoryBarrier(sift_buffer, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[oct_idx],
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[oct_idx]);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, oct_count, buffer_barriers, 0,
                       NULL);

  // Copy one indispatch buffer to the other
  // Prepare the access masks for the transfer
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[(oct_idx - oct_begin) * 2 + 0] = vkenv_genBufferMemoryBarrier(
        detector->mem->indirect_orientation_dispatch_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3);
    buffer_barriers[(oct_idx - oct_begin) * 2 + 1] =
        vkenv_genBufferMemoryBarrier(detector->mem->indirect_descriptor_dispatch_buffer, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED,
                                     VK_QUEUE_FAMILY_IGNORED, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, oct_count * 2, buffer_barriers, 0, NULL);

  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    VkBufferCopy region = {.srcOffset = detector->mem->indirect_oridesc_offset_arr[oct_idx],
                           .dstOffset = detector->mem->indirect_oridesc_offset_arr[oct_idx],
                           .size = sizeof(uint32_t) * 3};
    vkCmdCopyBuffer(cmdbuf, detector->mem->indirect_orientation_dispatch_buffer, detector->mem->indirect_descriptor_dispatch_buffer, 1, &region);
  }

  // Prepare for orientation pipeline dispatch buffer for the indirect dispatch call (require specific mask)
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[oct_idx - oct_begin] = vkenv_genBufferMemoryBarrier(
        detector->mem->indirect_orientation_dispatch_buffer, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, NULL, oct_count, buffer_barriers, 0, NULL);

  endMarkerRegion(detector, cmdbuf);

  free(buffer_barriers);
}

static void recComputeOrientationsCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_begin, const uint32_t oct_count)
{
  /////////////////////////////////////////////////
  // Compute orientation
  /////////////////////////////////////////////////
  VkBufferMemoryBarrier *buffer_barriers = (VkBufferMemoryBarrier *)malloc(sizeof(VkBufferMemoryBarrier) * oct_count);
  VkBuffer sift_buffer = detector->mem->sift_buffer_arr[detector->curr_buffer_idx];

  beginMarkerRegion(detector, cmdbuf, "ComputeOrientation");
  // Prepare the descriptor pipeline indirect dispatch buffer for writes access and make sure previous writes are visible
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[oct_idx - oct_begin] = vkenv_genBufferMemoryBarrier(detector->mem->indirect_descriptor_dispatch_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                        VK_ACCESS_SHADER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                                                        detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, oct_count, buffer_barriers, 0, NULL);

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->orientation_pipeline);
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->orientation_pipeline_layout, 0, 1, &detector->orientation_desc_sets[oct_idx],
                            0, NULL);
    vkCmdDispatchIndirect(cmdbuf, detector->mem->indirect_orientation_dispatch_buffer, detector->mem->indirect_oridesc_offset_arr[oct_idx]);
  }

  // Make sure writes are visible for future compute shaders
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[oct_idx - oct_begin] =
        vkenv_genBufferMemoryBarrier(sift_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[oct_idx],
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[oct_idx]);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, oct_count, buffer_barriers, 0,
                       NULL);

  // Prepare for descriptor pipeline dispatch buffer for the indirect dispatch call (require specific mask)
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[oct_idx - oct_begin] = vkenv_genBufferMemoryBarrier(
        detector->mem->indirect_descriptor_dispatch_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, detector->mem->indirect_oridesc_offset_arr[oct_idx], sizeof(uint32_t) * 3);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, NULL, oct_count, buffer_barriers, 0, NULL);

  endMarkerRegion(detector, cmdbuf);

  free(buffer_barriers);
}

static void recComputeDestriptorsCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_begin, const uint32_t oct_count)
{
  /////////////////////////////////////////////////
  // Compute descriptor
  /////////////////////////////////////////////////
  beginMarkerRegion(detector, cmdbuf, "ComputeDescriptors");
  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->descriptor_pipeline);

  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, detector->descriptor_pipeline_layout, 0, 1, &detector->descriptor_desc_sets[oct_idx],
                            0, NULL);
    vkCmdDispatchIndirect(cmdbuf, detector->mem->indirect_descriptor_dispatch_buffer, detector->mem->indirect_oridesc_offset_arr[oct_idx]);
  }
  endMarkerRegion(detector, cmdbuf);
}

static void recCopySIFTCountCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_begin, const uint32_t oct_count)
{
  /////////////////////////////////////////////////
  // Copy SIFT count to staging
  /////////////////////////////////////////////////
  VkBufferMemoryBarrier *buffer_barriers = (VkBufferMemoryBarrier *)malloc(sizeof(VkBufferMemoryBarrier) * oct_count);
  VkBuffer sift_buffer = detector->mem->sift_buffer_arr[detector->curr_buffer_idx];

  beginMarkerRegion(detector, cmdbuf, "CopySiftCount");

  // Only copy the number of detected SIFT features to the staging buffer (accessible by host)
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    buffer_barriers[oct_idx - oct_begin] = vkenv_genBufferMemoryBarrier(
        sift_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[oct_idx],
        detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[oct_idx]);
  }
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, oct_count, buffer_barriers, 0, NULL);

  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    VkBufferCopy sift_copy_region = {.srcOffset = detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[oct_idx],
                                     .dstOffset = sizeof(uint32_t) * oct_idx,
                                     .size = sizeof(uint32_t)};
    vkCmdCopyBuffer(cmdbuf, sift_buffer, detector->mem->sift_count_staging_buffer_arr[detector->curr_buffer_idx], 1, &sift_copy_region);
  }
  endMarkerRegion(detector, cmdbuf);

  free(buffer_barriers);
}

static void recBufferOwnershipTransferCmds(vksift_SiftDetector detector, VkCommandBuffer cmdbuf, const uint32_t oct_begin, const uint32_t oct_count,
                                           const uint32_t src_queue_family_idx, const uint32_t dst_queue_family_idx, VkPipelineStageFlags src_stage,
                                           VkPipelineStageFlags dst_stage)
{
  beginMarkerRegion(detector, cmdbuf, "BufferOwnershipTransfer");

  VkBufferMemoryBarrier *ownership_barriers = (VkBufferMemoryBarrier *)malloc(sizeof(VkBufferMemoryBarrier) * oct_count);
  for (uint32_t oct_idx = oct_begin; oct_idx < (oct_begin + oct_count); oct_idx++)
  {
    ownership_barriers[oct_idx - oct_begin] =
        vkenv_genBufferMemoryBarrier(detector->mem->sift_buffer_arr[detector->curr_buffer_idx], 0, 0, src_queue_family_idx, dst_queue_family_idx,
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_offset_arr[oct_idx],
                                     detector->mem->sift_buffers_info[detector->curr_buffer_idx].octave_section_size_arr[oct_idx]);
  }
  vkCmdPipelineBarrier(cmdbuf, src_stage, dst_stage, 0, 0, NULL, oct_count, ownership_barriers, 0, NULL);
  free(ownership_barriers);

  endMarkerRegion(detector, cmdbuf);
}

static bool recordCommandBuffers(vksift_SiftDetector detector)
{
  VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0, .pInheritanceInfo = NULL};

  // Write the empty end-of-detection command buffer used to signal that the detection work is done
  if (vkBeginCommandBuffer(detector->end_of_detection_command_buffer, &begin_info) != VK_SUCCESS ||
      vkEndCommandBuffer(detector->end_of_detection_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to record end-of-detection command buffer");
    return false;
  }

  /////////////////////////////////////////////////////
  // If async transfer queue is used, record ownership transfer command buffers
  /////////////////////////////////////////////////////
  if (detector->dev->async_transfer_available)
  {
    if (vkBeginCommandBuffer(detector->release_buffer_ownership_command_buffer, &begin_info) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to begin the release-buffer-ownership command buffer recording");
      return false;
    }
    recBufferOwnershipTransferCmds(detector, detector->release_buffer_ownership_command_buffer, 0, detector->mem->curr_nb_octaves,
                                   detector->dev->async_transfer_queues_family_idx, detector->dev->general_queues_family_idx,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if (vkEndCommandBuffer(detector->release_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to record release-buffer-ownership command buffer");
      return false;
    }

    if (vkBeginCommandBuffer(detector->acquire_buffer_ownership_command_buffer, &begin_info) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to begin the acquire-buffer-ownership command buffer recording");
      return false;
    }
    recBufferOwnershipTransferCmds(detector, detector->acquire_buffer_ownership_command_buffer, 0, detector->mem->curr_nb_octaves,
                                   detector->dev->general_queues_family_idx, detector->dev->async_transfer_queues_family_idx,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if (vkEndCommandBuffer(detector->acquire_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to record acquire-buffer-ownership command buffer");
      return false;
    }
  }

  /////////////////////////////////////////////////////
  // Write the detection command buffer (single queue version)
  /////////////////////////////////////////////////////
  if (vkBeginCommandBuffer(detector->detection_command_buffer, &begin_info) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to begin the command buffer recording");
    return false;
  }

  // We start using the SIFT buffer, is the async transfer is used we need to acquire the buffer ownership before using it
  if (detector->dev->async_transfer_available)
  {
    recBufferOwnershipTransferCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves,
                                   detector->dev->async_transfer_queues_family_idx, detector->dev->general_queues_family_idx,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }

  // Clear buffer data
  recClearBufferDataCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves);
  // Copy input image
  recCopyInputImageCmds(detector, detector->detection_command_buffer);

  // Scale space construction
  for (uint32_t i = 0; i < detector->mem->curr_nb_octaves; i++)
  {
    // Construct each octave
    recScaleSpaceConstructionCmds(detector, detector->detection_command_buffer, i);
  }

  // Compute difference of Gaussian (on full range to synchronize every octave with a single barrier)
  recDifferenceOfGaussianCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves);

  // Extract extrema (keypoints) from DoG images
  recExtractKeypointsCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves);
  // For the main orientations of each keypoint (this creates new keypoints if there's more than one orientation)
  recComputeOrientationsCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves);

  // For each oriented keypoint compute its descriptor
  recComputeDestriptorsCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves);

  // Copy the number of found keypoints to the sift_count staging buffer
  // (so that when the CPU want to download the result it can download only the number of SIFT found with a custom command buffer)
  recCopySIFTCountCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves);

  // No more operation with the buffer we can release the buffer ownership if needed
  if (detector->dev->async_transfer_available)
  {
    recBufferOwnershipTransferCmds(detector, detector->detection_command_buffer, 0, detector->mem->curr_nb_octaves,
                                   detector->dev->general_queues_family_idx, detector->dev->async_transfer_queues_family_idx,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  }

  if (vkEndCommandBuffer(detector->detection_command_buffer) != VK_SUCCESS)
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

  // Assign queues
  detector->general_queue = device->general_queues[0];
  if (device->async_transfer_available)
  {
    detector->async_ownership_transfer_queue = device->async_transfer_queues[1]; // queue 0 used by SiftMemory only
  }

  // Retrieve config
  detector->use_hardware_interp_kernel = config->use_hardware_interpolated_blur;
  detector->input_blur_level = config->input_image_blur_level;
  detector->seed_scale_sigma = config->seed_scale_sigma;
  detector->intensity_threshold = config->intensity_threshold;
  detector->edge_threshold = config->edge_threshold;

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

bool vksift_dispatchSiftDetection(vksift_SiftDetector detector, const uint32_t target_buffer_idx, const bool memory_layout_updated)
{
  // We need to setup the descriptor sets and command buffers if the input resolution or target buffer changed
  if (memory_layout_updated || detector->curr_buffer_idx != target_buffer_idx)
  {
    detector->curr_buffer_idx = target_buffer_idx;
    writeDescriptorSets(detector);
    recordCommandBuffers(detector);
    // logError(LOG_TAG, "rewrite stuff");
  }

  // Mark the buffer as busy/GPU locked
  vkResetFences(detector->dev->device, 1, &detector->mem->sift_buffer_fence_arr[detector->curr_buffer_idx]);
  vkResetFences(detector->dev->device, 1, &detector->end_of_detection_fence);

  VkPipelineStageFlags wait_dst_transfer_bit_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkPipelineStageFlags wait_dst_compute_shader_bit_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

  VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = NULL};

  if (detector->dev->async_transfer_available)
  {
    // Transfer SIFT buffer ownership for the detection
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &detector->release_buffer_ownership_command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &detector->buffer_ownership_released_by_transfer_semaphore;
    if (vkQueueSubmit(detector->async_ownership_transfer_queue, 1, &submit_info, NULL) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to submit ownership-release command buffer on async transfer queue");
      return false;
    }
  }

  // Main detection
  if (detector->dev->async_transfer_available)
  {
    // wait for buffer ownership transfer to complete
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &detector->buffer_ownership_released_by_transfer_semaphore;
    submit_info.pWaitDstStageMask = &wait_dst_compute_shader_bit_stage_mask;
  }
  else
  {
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
  }
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &detector->detection_command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &detector->end_of_detection_semaphore;
  if (vkQueueSubmit(detector->general_queue, 1, &submit_info, detector->mem->sift_buffer_fence_arr[detector->curr_buffer_idx]) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to submit detection command buffer");
    return false;
  }

  if (detector->dev->async_transfer_available)
  {
    // Give back SIFT buffer ownership to the main memory
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &detector->end_of_detection_semaphore;
    submit_info.pWaitDstStageMask = &wait_dst_transfer_bit_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &detector->acquire_buffer_ownership_command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &detector->buffer_ownership_acquired_by_transfer_semaphore;
    if (vkQueueSubmit(detector->async_ownership_transfer_queue, 1, &submit_info, NULL) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to submit ownership-release command buffer on async transfer queue");
      return false;
    }
  }

  // Final command buffer (used only to signal the end-of-detection fence)
  if (detector->dev->async_transfer_available)
  {
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &detector->buffer_ownership_acquired_by_transfer_semaphore;
    submit_info.pWaitDstStageMask = &wait_dst_transfer_bit_stage_mask;
  }
  else
  {
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &detector->end_of_detection_semaphore;
    submit_info.pWaitDstStageMask = &wait_dst_transfer_bit_stage_mask;
  }
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &detector->end_of_detection_command_buffer;
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = NULL;
  if (vkQueueSubmit(detector->general_queue, 1, &submit_info, detector->end_of_detection_fence) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to submit command buffer");
    return false;
  }

  return true;
}

void vksift_destroySiftDetector(vksift_SiftDetector *detector_ptr)
{
  assert(detector_ptr != NULL);
  assert(*detector_ptr != NULL); // vksift_destroySiftDetector shouldn't be called on NULL vksift_SiftMemory object
  vksift_SiftDetector detector = *detector_ptr;

  // Destroy sampler
  VK_NULL_SAFE_DELETE(detector->image_sampler, vkDestroySampler(detector->dev->device, detector->image_sampler, NULL));

  // Destroy sync objects
  VK_NULL_SAFE_DELETE(detector->end_of_detection_semaphore, vkDestroySemaphore(detector->dev->device, detector->end_of_detection_semaphore, NULL));
  VK_NULL_SAFE_DELETE(detector->end_of_detection_fence, vkDestroyFence(detector->dev->device, detector->end_of_detection_fence, NULL));
  if (detector->dev->async_transfer_available)
  {
    VK_NULL_SAFE_DELETE(detector->buffer_ownership_released_by_transfer_semaphore,
                        vkDestroySemaphore(detector->dev->device, detector->buffer_ownership_released_by_transfer_semaphore, NULL));
    VK_NULL_SAFE_DELETE(detector->buffer_ownership_acquired_by_transfer_semaphore,
                        vkDestroySemaphore(detector->dev->device, detector->buffer_ownership_acquired_by_transfer_semaphore, NULL));
  }

  // Destroy command pools
  VK_NULL_SAFE_DELETE(detector->general_command_pool, vkDestroyCommandPool(detector->dev->device, detector->general_command_pool, NULL));
  if (detector->dev->async_transfer_available)
  {
    VK_NULL_SAFE_DELETE(detector->async_transfer_command_pool, vkDestroyCommandPool(detector->dev->device, detector->async_transfer_command_pool, NULL));
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
  // Extract keypoints
  VK_NULL_SAFE_DELETE(detector->extractkpts_pipeline, vkDestroyPipeline(detector->dev->device, detector->extractkpts_pipeline, NULL));
  VK_NULL_SAFE_DELETE(detector->extractkpts_pipeline_layout, vkDestroyPipelineLayout(detector->dev->device, detector->extractkpts_pipeline_layout, NULL));
  VK_NULL_SAFE_DELETE(detector->extractkpts_desc_pool, vkDestroyDescriptorPool(detector->dev->device, detector->extractkpts_desc_pool, NULL));
  VK_NULL_SAFE_DELETE(detector->extractkpts_desc_set_layout,
                      vkDestroyDescriptorSetLayout(detector->dev->device, detector->extractkpts_desc_set_layout, NULL));
  // Compute orientation
  VK_NULL_SAFE_DELETE(detector->orientation_pipeline, vkDestroyPipeline(detector->dev->device, detector->orientation_pipeline, NULL));
  VK_NULL_SAFE_DELETE(detector->orientation_pipeline_layout, vkDestroyPipelineLayout(detector->dev->device, detector->orientation_pipeline_layout, NULL));
  VK_NULL_SAFE_DELETE(detector->orientation_desc_pool, vkDestroyDescriptorPool(detector->dev->device, detector->orientation_desc_pool, NULL));
  VK_NULL_SAFE_DELETE(detector->orientation_desc_set_layout,
                      vkDestroyDescriptorSetLayout(detector->dev->device, detector->orientation_desc_set_layout, NULL));
  // Compute descriptor
  VK_NULL_SAFE_DELETE(detector->descriptor_pipeline, vkDestroyPipeline(detector->dev->device, detector->descriptor_pipeline, NULL));
  VK_NULL_SAFE_DELETE(detector->descriptor_pipeline_layout, vkDestroyPipelineLayout(detector->dev->device, detector->descriptor_pipeline_layout, NULL));
  VK_NULL_SAFE_DELETE(detector->descriptor_desc_pool, vkDestroyDescriptorPool(detector->dev->device, detector->descriptor_desc_pool, NULL));
  VK_NULL_SAFE_DELETE(detector->descriptor_desc_set_layout,
                      vkDestroyDescriptorSetLayout(detector->dev->device, detector->descriptor_desc_set_layout, NULL));

  // Free descriptor arrays
  VK_NULL_SAFE_DELETE(detector->gaussian_kernels, free(detector->gaussian_kernels));
  VK_NULL_SAFE_DELETE(detector->gaussian_kernel_sizes, free(detector->gaussian_kernel_sizes));
  VK_NULL_SAFE_DELETE(detector->blur_desc_sets, free(detector->blur_desc_sets));
  VK_NULL_SAFE_DELETE(detector->dog_desc_sets, free(detector->dog_desc_sets));
  VK_NULL_SAFE_DELETE(detector->extractkpts_desc_sets, free(detector->extractkpts_desc_sets));
  VK_NULL_SAFE_DELETE(detector->orientation_desc_sets, free(detector->orientation_desc_sets));
  VK_NULL_SAFE_DELETE(detector->descriptor_desc_sets, free(detector->descriptor_desc_sets));

  // Release vksift_SiftDetector memory
  free(*detector_ptr);
  *detector_ptr = NULL;
}