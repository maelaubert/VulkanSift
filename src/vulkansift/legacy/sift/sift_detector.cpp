#include "vulkansift/sift/sift_detector.h"

#include "vulkansift/utils/logger.h"

#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace VulkanSIFT
{

static char LOG_TAG[] = "SiftDetector";

constexpr uint32_t MAX_GAUSSIAN_KERNEL_SIZE = 20u;

bool SiftDetector::init(VulkanInstance *vulkan_instance, const int image_width, const int image_height)
{
  m_image_width = image_width;
  m_image_height = image_height;

  // m_nb_octave = roundf(logf(static_cast<float>(std::min(image_width * 2, image_height * 2))) / logf(2.f) - 2) + 1;
  m_nb_octave = std::max(floorf(log2f(static_cast<float>(std::min(image_width, image_height))) + 1 - 3), 1.f);

  // Compute image sizes (per octave and scale)
  m_octave_image_sizes.clear();
  for (uint32_t oct_i = 0; oct_i < m_nb_octave; oct_i++)
  {
    uint32_t octave_width = (1.f / (powf(2.f, oct_i) * m_scale_factor_min)) * static_cast<float>(m_image_width);
    uint32_t octave_height = (1.f / (powf(2.f, oct_i) * m_scale_factor_min)) * static_cast<float>(m_image_height);
    m_octave_image_sizes.push_back({octave_width, octave_height});
  }

  // Compute the max amount of feature per octave
  m_max_nb_feat_per_octave.clear();
  {
    float sum = ((float)m_max_nb_sift / 2) * ((1 - powf(0.5f, m_nb_octave)) / (1 - 0.5f));
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      float nb_curr_oct = (((float)m_max_nb_sift * powf(0.5f, i + 1)) / sum) * (float)m_max_nb_sift;
      // float nb_curr_oct = (m_octave_image_sizes[i].width / 3) * (m_octave_image_sizes[i].height / 3) * m_nb_scale_per_oct * 3;
      m_max_nb_feat_per_octave.push_back(nb_curr_oct);
    }
  }

  // Precompute gaussian kernels for scale space computation
  precomputeGaussianKernels();

  m_vulkan_instance = vulkan_instance;

  // Get Vulkan entities and information from VulkanManager
  m_physical_device = m_vulkan_instance->getVkPhysicalDevice();
  if (m_physical_device == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanManager returned a NULL physical device.");
    return false;
  }
  m_physical_device_props = m_vulkan_instance->getVkPhysicalDeviceProperties();

  m_device = m_vulkan_instance->getVkDevice();
  if (m_device == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanManager returned a NULL logical device.");
    return false;
  }
  m_queue_family_index = m_vulkan_instance->getGraphicsQueueFamilyIndex();
  m_queue = m_vulkan_instance->getGraphicsQueue();
  if (m_queue == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanManager returned a NULL queue.");
    terminate();
    return false;
  }

  m_debug_marker_supported = m_vulkan_instance->isDebugMarkerSupported();

  // Init command pool
  if (!initCommandPool())
  {
    terminate();
    return false;
  }

  // Init memory objects (buffers/images)
  if (!initMemory())
  {
    terminate();
    return false;
  }

  // Init image sampler object
  if (!initSampler())
  {
    terminate();
    return false;
  }

  // Init descriptors used to access the objects in shaders
  if (!initDescriptors())
  {
    terminate();
    return false;
  }

  // Init compute pipelines with shaders
  if (!initPipelines())
  {
    terminate();
    return false;
  }

  VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
  vkCreateFence(m_device, &fence_info, nullptr, &m_fence);

  // Init command buffers ran during calls to compute()
  if (!initCommandBuffer())
  {
    terminate();
    return false;
  }

  return true;
}

void SiftDetector::precomputeGaussianKernels()
{
  m_gaussian_kernels.resize(m_nb_scale_per_oct + 3);
  for (uint32_t scale_i = 0; scale_i < m_nb_scale_per_oct + 3; scale_i++)
  {
    float sep_kernel_sigma;
    if (scale_i == 0)
    {
      // Used only for first octave (since all other first scales used downsampled scale from octave-1)
      sep_kernel_sigma = sqrtf((m_sigma_min * m_sigma_min) - (m_sigma_in * m_sigma_in * 4));
    }
    else
    {
      // Gaussian blur from one scale to the other
      float sig_prev = std::pow(std::pow(2.f, 1.f / 3), static_cast<float>(scale_i - 1)) * m_sigma_min;
      float sig_total = sig_prev * std::pow(2.f, 1.f / 3);
      sep_kernel_sigma = std::sqrt(sig_total * sig_total - sig_prev * sig_prev);
    }

    // Compute the gaussian kernel for the defined sigma value
    uint32_t kernel_size = static_cast<int>(ceilf(sep_kernel_sigma * 4.f) + 1.f);
    kernel_size = std::min(kernel_size, MAX_GAUSSIAN_KERNEL_SIZE);

    std::vector<float> kernel_data;
    kernel_data.resize(kernel_size);
    kernel_data[0] = 1.f;
    float sum_kernel = kernel_data[0];
    for (uint32_t i = 1; i < kernel_size; i++)
    {
      kernel_data[i] = exp(-0.5 * pow(float(i), 2.f) / pow(sep_kernel_sigma, 2.f));
      sum_kernel += 2 * kernel_data[i];
    }
    for (uint32_t i = 0; i < kernel_size; i++)
    {
      kernel_data[i] /= sum_kernel;
    }

    // Compute hardware interpolated kernel if necessary
    if (m_use_hardware_interp_kernel)
    {
      // Hardware interpolated kernel based on https://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
      // Goal here is to use hardware sampler to reduce the number of texture fetch by a factor of two
      uint32_t hardware_interp_size = (uint32_t)(ceilf((float)kernel_size / 2.f));
      m_gaussian_kernels[scale_i].resize(hardware_interp_size * 2); // Twice the space to store both coeffs and offsets

      // First kernel coeff and offset stays the same
      m_gaussian_kernels[scale_i][0] = kernel_data[0];
      m_gaussian_kernels[scale_i][1] = (float)0;
      for (uint32_t data_idx = 1, kern_idx = 1; (data_idx + 1) < kernel_size; data_idx += 2, kern_idx++)
      {
        m_gaussian_kernels[scale_i][kern_idx * 2] = kernel_data[data_idx] + kernel_data[data_idx + 1];
        m_gaussian_kernels[scale_i][(kern_idx * 2) + 1] =
            (((float)data_idx * kernel_data[data_idx]) + ((float)(data_idx + 1) * kernel_data[data_idx + 1])) /
            (kernel_data[data_idx] + kernel_data[data_idx + 1]);
      }
    }
    else
    {
      m_gaussian_kernels[scale_i].resize(kernel_size);
      for (uint32_t i = 0; i < kernel_size; i++)
      {
        m_gaussian_kernels[scale_i][i] = kernel_data[i];
      }
    }
    // logInfo(LOG_TAG, "Scale %d: sigma=%f | kernel size=%d", scale_i, sep_kernel_sigma, kernel_size);
  }
}

bool SiftDetector::initCommandPool()
{
  VkCommandPoolCreateInfo pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = 0, .queueFamilyIndex = m_queue_family_index};
  if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create command pool");
    return false;
  }

  return true;
}

bool SiftDetector::initMemory()
{
  // Need staging buffer to upload user image data to device only mem
  {
    VkDeviceSize buffer_size = sizeof(uint8_t) * m_image_width * m_image_height;
    if (!m_input_image_staging_in_buffer.create(m_device, m_physical_device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
    {
      logError(LOG_TAG, "Failed to create input image staging buffer.");
      return false;
    }
  }

  // Need input image
  {

    if (!m_input_image.create(m_device, m_physical_device, m_image_width, m_image_height, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
      logError(LOG_TAG, "Failed to create input image.");
      return false;
    }
  }

  // Need temporary image of max image size to store gaussian blur horizontal pass temporary result
  {
    m_blur_temp_results.resize(m_nb_octave);
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_blur_temp_results[i].create(
              m_device, m_physical_device, m_octave_image_sizes[i].width, m_octave_image_sizes[i].height, VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1))
      {
        logError(LOG_TAG, "Failed to create blur temporary result image.");
        return false;
      }
    }
  }

  // Need per octave images since all scales should be accessible per shader invocation (especially when finding points)
  {
    m_octave_images.resize(m_nb_octave);
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_octave_images[i].create(
              m_device, m_physical_device, m_octave_image_sizes[i].width, m_octave_image_sizes[i].height, VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_nb_scale_per_oct + 3))
      {
        logError(LOG_TAG, "Failed to create octave image.");
        return false;
      }
    }
  }

  // Need per octave DoG images
  {
    m_octave_DoG_images.resize(m_nb_octave);
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_octave_DoG_images[i].create(
              m_device, m_physical_device, m_octave_image_sizes[i].width, m_octave_image_sizes[i].height, VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
              // m_octave_image_sizes[i].height * (m_nb_scale_per_oct + 2), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_nb_scale_per_oct + 2))
      {
        logError(LOG_TAG, "Failed to create DoG octave image.");
        return false;
      }
    }
  }

  // Need IndirectDispatch buffers info for orientation and descriptor dispatch
  {
    m_indispatch_orientation_buffers.resize(m_nb_octave);
    VkDeviceSize buffer_size = sizeof(uint32_t) * 3;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_indispatch_orientation_buffers[i].create(m_device, m_physical_device, buffer_size,
                                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        logError(LOG_TAG, "Failed to create orientation indirect dispatch buffer.");
        return false;
      }
    }
  }
  {
    m_indispatch_descriptors_buffers.resize(m_nb_octave);
    VkDeviceSize buffer_size = sizeof(uint32_t) * 3;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_indispatch_descriptors_buffers[i].create(m_device, m_physical_device, buffer_size,
                                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        logError(LOG_TAG, "Failed to create descriptors indirect dispatch buffer.");
        return false;
      }
    }
  }

  // Create buffer to store SIFT data
  {
    m_sift_keypoints_buffers.resize(m_nb_octave);
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_sift_keypoints_buffers[i].create(m_device, m_physical_device, sizeof(uint32_t) + (m_max_nb_feat_per_octave[i] * sizeof(SIFT_Feature)),
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        logError(LOG_TAG, "Failed to create SIFT buffer storage.");
        return false;
      }
    }
  }

  // Create staging buffer to send back SIFT data to CPU
  {
    m_sift_staging_out_buffers.resize(m_nb_octave);
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_sift_staging_out_buffers[i].create(m_device, m_physical_device, sizeof(uint32_t) + (m_max_nb_feat_per_octave[i] * sizeof(SIFT_Feature)),
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
      {
        logError(LOG_TAG, "Failed to create SIFT buffer storage.");
        return false;
      }
    }
  }

  // Apply initial layouts to images and buffers
  bool cmd_res = VulkanUtils::submitCommandsAndWait(m_device, m_queue, m_command_pool, [&](VkCommandBuffer command_buf) {
    std::vector<VkImageMemoryBarrier> image_barriers;

    // Setup TOP_OF_PIPE -> COMPUTE image layouts
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      image_barriers.push_back(m_octave_images[i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
      image_barriers.push_back(m_blur_temp_results[i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
      image_barriers.push_back(m_octave_DoG_images[i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
    }
    image_barriers.push_back(m_input_image.getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
    vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         image_barriers.size(), image_barriers.data());
  });
  if (!cmd_res)
  {
    logError(LOG_TAG, "Failed to apply barriers to images after creation");
    return false;
  }

  // Map memory for CPU side access (to provide input image and retrieve output data)
  if (!m_input_image_staging_in_buffer.mapMemory(m_device, 0, m_image_width * m_image_height * sizeof(uint8_t), 0, &m_input_image_ptr))
  {
    logError(LOG_TAG, "Failed to map input buffer memory.");
    return false;
  }

  m_output_sift_ptr.resize(m_nb_octave);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    if (!m_sift_staging_out_buffers[i].mapMemory(m_device, 0, VK_WHOLE_SIZE, 0, &m_output_sift_ptr[i]))
    {
      logError(LOG_TAG, "Failed to map output buffer memory.");
      return false;
    }
  }

  return true;
}

bool SiftDetector::initSampler()
{

  VkSamplerCreateInfo sampler_info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                   .pNext = nullptr,
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

  if (vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create image sampler");
    return false;
  }
  return true;
}

bool SiftDetector::initDescriptors()
{

  ///////////////////////////////////////////////////
  // Descriptors for GaussianBlur pipeline
  ///////////////////////////////////////////////////
  {
    VkDescriptorSetLayoutBinding blur_input_layout_binding{.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                           .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding blur_output_layout_binding{.binding = 1,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .pImmutableSamplers = nullptr};
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{blur_input_layout_binding, blur_output_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_blur_desc_set_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create GaussianBlur descriptor set layout");
      return false;
    }

    // Create descriptor pool to allocate descriptor sets (generic)
    std::array<VkDescriptorPoolSize, 2> pool_sizes;
    pool_sizes[0] = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = m_nb_octave * 2};
    pool_sizes[1] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = m_nb_octave * 2};
    VkDescriptorPoolCreateInfo descriptor_pool_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .maxSets = m_nb_octave * 2,
                                                    .poolSizeCount = pool_sizes.size(),
                                                    .pPoolSizes = pool_sizes.data()};
    if (vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_blur_desc_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create GaussianBlur descriptor pool");
      return false;
    }

    // Create descriptor sets that can be bound in command buffer
    std::vector<VkDescriptorSetLayout> layouts{m_nb_octave, m_blur_desc_set_layout};
    m_blur_h_desc_sets.resize(m_nb_octave);
    m_blur_v_desc_sets.resize(m_nb_octave);
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = m_blur_desc_pool,
                                           .descriptorSetCount = m_nb_octave,
                                           .pSetLayouts = layouts.data()};

    if (vkAllocateDescriptorSets(m_device, &alloc_info, m_blur_h_desc_sets.data()) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate horizontal GaussianBlur descriptor set");
      return false;
    }
    if (vkAllocateDescriptorSets(m_device, &alloc_info, m_blur_v_desc_sets.data()) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate vertical GaussianBlur descriptor set");
      return false;
    }

    // Write descriptor sets
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      VkDescriptorImageInfo blur_input_image_info{
          .sampler = m_sampler, .imageView = m_octave_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorImageInfo blur_work_image_info{
          .sampler = m_sampler, .imageView = m_blur_temp_results[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorImageInfo blur_output_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      std::array<VkWriteDescriptorSet, 2> descriptor_writes;
      // First write for horizontal pass
      descriptor_writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_blur_h_desc_sets[i],
                              .dstBinding = 0,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                              .pImageInfo = &blur_input_image_info,
                              .pBufferInfo = nullptr,
                              .pTexelBufferView = nullptr};
      descriptor_writes[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_blur_h_desc_sets[i],
                              .dstBinding = 1,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                              .pImageInfo = &blur_work_image_info,
                              .pBufferInfo = nullptr,
                              .pTexelBufferView = nullptr};
      vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
      // Then write for vertical pass
      descriptor_writes[0].dstSet = m_blur_v_desc_sets[i];
      descriptor_writes[0].pImageInfo = &blur_work_image_info;
      descriptor_writes[1].dstSet = m_blur_v_desc_sets[i];
      descriptor_writes[1].pImageInfo = &blur_output_image_info;
      vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
  }

  ///////////////////////////////////////////////////
  // Descriptors for DifferenceOfGaussian pipeline
  ///////////////////////////////////////////////////
  {
    VkDescriptorSetLayoutBinding dog_input_layout_binding{.binding = 0,
                                                          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                          .descriptorCount = 1,
                                                          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                          .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding dog_output_layout_binding{.binding = 1,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                           .pImmutableSamplers = nullptr};
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{dog_input_layout_binding, dog_output_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_dog_desc_set_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian descriptor set layout");
      return false;
    }

    // Create descriptor pool to allocate descriptor sets (generic)
    std::array<VkDescriptorPoolSize, 2> pool_sizes;
    pool_sizes[0] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = m_nb_octave};
    pool_sizes[1] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = m_nb_octave};
    VkDescriptorPoolCreateInfo descriptor_pool_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .maxSets = m_nb_octave,
                                                    .poolSizeCount = pool_sizes.size(),
                                                    .pPoolSizes = pool_sizes.data()};
    if (vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_dog_desc_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian descriptor pool");
      return false;
    }

    // Create descriptor sets that can be bound in command buffer
    std::vector<VkDescriptorSetLayout> layouts{m_nb_octave, m_dog_desc_set_layout};
    m_dog_desc_sets.resize(m_nb_octave);
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = m_dog_desc_pool,
                                           .descriptorSetCount = m_nb_octave,
                                           .pSetLayouts = layouts.data()};

    if (vkAllocateDescriptorSets(m_device, &alloc_info, m_dog_desc_sets.data()) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate DifferenceOfGaussian descriptor set");
      return false;
    }

    // Write descriptor sets
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      VkDescriptorImageInfo dog_input_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorImageInfo dog_output_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_DoG_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      std::array<VkWriteDescriptorSet, 2> descriptor_writes;
      descriptor_writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_dog_desc_sets[i],
                              .dstBinding = 0,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                              .pImageInfo = &dog_input_image_info,
                              .pBufferInfo = nullptr,
                              .pTexelBufferView = nullptr};
      descriptor_writes[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_dog_desc_sets[i],
                              .dstBinding = 1,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                              .pImageInfo = &dog_output_image_info,
                              .pBufferInfo = nullptr,
                              .pTexelBufferView = nullptr};
      vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
  }
  ///////////////////////////////////////////////////
  // Descriptors for ExtractKeypoints pipeline
  ///////////////////////////////////////////////////
  {
    VkDescriptorSetLayoutBinding dog_image_layout_binding{.binding = 0,
                                                          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                          .descriptorCount = 1,
                                                          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                          .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding sift_buffer_layout_binding{.binding = 1,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding indispatch_buffer_layout_binding{.binding = 2,
                                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                  .descriptorCount = 1,
                                                                  .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                  .pImmutableSamplers = nullptr};

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{dog_image_layout_binding, sift_buffer_layout_binding, indispatch_buffer_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_extractkpts_desc_set_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ExtractKeypoints descriptor set layout");
      return false;
    }

    // Create descriptor pool to allocate descriptor sets (generic)
    std::array<VkDescriptorPoolSize, 3> pool_sizes;
    pool_sizes[0] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = m_nb_octave};
    pool_sizes[1] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = m_nb_octave};
    pool_sizes[2] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = m_nb_octave};
    VkDescriptorPoolCreateInfo descriptor_pool_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .maxSets = m_nb_octave,
                                                    .poolSizeCount = pool_sizes.size(),
                                                    .pPoolSizes = pool_sizes.data()};
    if (vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_extractkpts_desc_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ExtractKeypoints descriptor pool");
      return false;
    }

    // Create descriptor sets that can be bound in command buffer
    std::vector<VkDescriptorSetLayout> layouts{m_nb_octave, m_extractkpts_desc_set_layout};
    m_extractkpts_desc_sets.resize(m_nb_octave);
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = m_extractkpts_desc_pool,
                                           .descriptorSetCount = m_nb_octave,
                                           .pSetLayouts = layouts.data()};

    if (vkAllocateDescriptorSets(m_device, &alloc_info, m_extractkpts_desc_sets.data()) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate ExtractKeypoints descriptor set");
      return false;
    }

    // Write descriptor sets
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      VkDescriptorImageInfo dog_input_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_DoG_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorBufferInfo sift_buffer_info{.buffer = m_sift_keypoints_buffers[i].getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo indispatch_buffer_info{.buffer = m_indispatch_orientation_buffers[i].getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      std::array<VkWriteDescriptorSet, 3> descriptor_writes;
      descriptor_writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_extractkpts_desc_sets[i],
                              .dstBinding = 0,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                              .pImageInfo = &dog_input_image_info,
                              .pBufferInfo = nullptr,
                              .pTexelBufferView = nullptr};
      descriptor_writes[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_extractkpts_desc_sets[i],
                              .dstBinding = 1,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              .pImageInfo = nullptr,
                              .pBufferInfo = &sift_buffer_info,
                              .pTexelBufferView = nullptr};
      descriptor_writes[2] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_extractkpts_desc_sets[i],
                              .dstBinding = 2,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              .pImageInfo = nullptr,
                              .pBufferInfo = &indispatch_buffer_info,
                              .pTexelBufferView = nullptr};
      vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
  }
  ///////////////////////////////////////////////////
  // Descriptors for ComputeOrientation pipeline
  ///////////////////////////////////////////////////
  {
    VkDescriptorSetLayoutBinding octave_image_layout_binding{.binding = 0,
                                                             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                             .descriptorCount = 1,
                                                             .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                             .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding sift_buffer_layout_binding{.binding = 1,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding indispatch_buffer_layout_binding{.binding = 2,
                                                                  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                  .descriptorCount = 1,
                                                                  .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                  .pImmutableSamplers = nullptr};

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{octave_image_layout_binding, sift_buffer_layout_binding, indispatch_buffer_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_orientation_desc_set_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeOrientation descriptor set layout");
      return false;
    }

    // Create descriptor pool to allocate descriptor sets (generic)
    std::array<VkDescriptorPoolSize, 3> pool_sizes;
    pool_sizes[0] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = m_nb_octave};
    pool_sizes[1] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = m_nb_octave};
    pool_sizes[2] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = m_nb_octave};
    VkDescriptorPoolCreateInfo descriptor_pool_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .maxSets = m_nb_octave,
                                                    .poolSizeCount = pool_sizes.size(),
                                                    .pPoolSizes = pool_sizes.data()};
    if (vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_orientation_desc_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeOrientation descriptor pool");
      return false;
    }

    // Create descriptor sets that can be bound in command buffer
    std::vector<VkDescriptorSetLayout> layouts{m_nb_octave, m_orientation_desc_set_layout};
    m_orientation_desc_sets.resize(m_nb_octave);
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = m_orientation_desc_pool,
                                           .descriptorSetCount = m_nb_octave,
                                           .pSetLayouts = layouts.data()};

    if (vkAllocateDescriptorSets(m_device, &alloc_info, m_orientation_desc_sets.data()) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate ComputeOrientation descriptor set");
      return false;
    }

    // Write descriptor sets
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      VkDescriptorImageInfo octave_input_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorBufferInfo sift_buffer_info{.buffer = m_sift_keypoints_buffers[i].getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo indispatch_buffer_info{.buffer = m_indispatch_descriptors_buffers[i].getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      std::array<VkWriteDescriptorSet, 3> descriptor_writes;
      descriptor_writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_orientation_desc_sets[i],
                              .dstBinding = 0,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                              .pImageInfo = &octave_input_image_info,
                              .pBufferInfo = nullptr,
                              .pTexelBufferView = nullptr};
      descriptor_writes[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_orientation_desc_sets[i],
                              .dstBinding = 1,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              .pImageInfo = nullptr,
                              .pBufferInfo = &sift_buffer_info,
                              .pTexelBufferView = nullptr};
      descriptor_writes[2] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_orientation_desc_sets[i],
                              .dstBinding = 2,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              .pImageInfo = nullptr,
                              .pBufferInfo = &indispatch_buffer_info,
                              .pTexelBufferView = nullptr};
      vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
  }
  ///////////////////////////////////////////////////
  // Descriptors for ComputeDescriptors pipeline
  ///////////////////////////////////////////////////
  {
    VkDescriptorSetLayoutBinding octave_image_layout_binding{.binding = 0,
                                                             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                             .descriptorCount = 1,
                                                             .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                             .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding sift_buffer_layout_binding{.binding = 1,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .pImmutableSamplers = nullptr};

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{octave_image_layout_binding, sift_buffer_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_descriptor_desc_set_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeDescriptors descriptor set layout");
      return false;
    }

    // Create descriptor pool to allocate descriptor sets (generic)
    std::array<VkDescriptorPoolSize, 2> pool_sizes;
    pool_sizes[0] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = m_nb_octave};
    pool_sizes[1] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = m_nb_octave};
    VkDescriptorPoolCreateInfo descriptor_pool_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .maxSets = m_nb_octave,
                                                    .poolSizeCount = pool_sizes.size(),
                                                    .pPoolSizes = pool_sizes.data()};
    if (vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_descriptor_desc_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeDescriptors descriptor pool");
      return false;
    }

    // Create descriptor sets that can be bound in command buffer
    std::vector<VkDescriptorSetLayout> layouts{m_nb_octave, m_descriptor_desc_set_layout};
    m_descriptor_desc_sets.resize(m_nb_octave);
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = m_descriptor_desc_pool,
                                           .descriptorSetCount = m_nb_octave,
                                           .pSetLayouts = layouts.data()};

    if (vkAllocateDescriptorSets(m_device, &alloc_info, m_descriptor_desc_sets.data()) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate ComputeDescriptors descriptor set");
      return false;
    }

    // Write descriptor sets
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      VkDescriptorImageInfo octave_input_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorBufferInfo sift_buffer_info{.buffer = m_sift_keypoints_buffers[i].getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      std::array<VkWriteDescriptorSet, 2> descriptor_writes;
      descriptor_writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_descriptor_desc_sets[i],
                              .dstBinding = 0,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                              .pImageInfo = &octave_input_image_info,
                              .pBufferInfo = nullptr,
                              .pTexelBufferView = nullptr};
      descriptor_writes[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_descriptor_desc_sets[i],
                              .dstBinding = 1,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              .pImageInfo = nullptr,
                              .pBufferInfo = &sift_buffer_info,
                              .pTexelBufferView = nullptr};
      vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
  }
  return true;
}

struct GaussianBlurPushConsts
{
  uint32_t is_vertical;
  uint32_t array_layer;
  uint32_t kernel_size;
  float kernel[MAX_GAUSSIAN_KERNEL_SIZE];
};

struct ExtractKeypointsPushConsts
{
  float scale_factor;
  float sigma_multiplier;
  float dog_threshold;
  float edge_threshold;
};

bool SiftDetector::initPipelines()
{
  //////////////////////////////////////
  // Setup GaussianBlur pipeline
  //////////////////////////////////////
  {
    VkShaderModule blur_shader_module;
    if (m_use_hardware_interp_kernel)
    {
      if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/GaussianBlurInterpolated.comp.spv", &blur_shader_module))
      {
        logError(LOG_TAG, "Failed to create GaussianBlurInterpolated shader module");
        return false;
      }
    }
    else if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/GaussianBlur.comp.spv", &blur_shader_module))
    {
      logError(LOG_TAG, "Failed to create GaussianBlur shader module");
      return false;
    }
    if (!VulkanUtils::createComputePipeline(m_device, blur_shader_module, m_blur_desc_set_layout, sizeof(GaussianBlurPushConsts), &m_blur_pipeline_layout,
                                            &m_blur_pipeline))
    {
      logError(LOG_TAG, "Failed to create GaussianBlur pipeline");
      vkDestroyShaderModule(m_device, blur_shader_module, nullptr);
      return false;
    }
    vkDestroyShaderModule(m_device, blur_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup DifferenceOfGaussian pipeline
  //////////////////////////////////////
  {
    VkShaderModule dog_shader_module;
    if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/DifferenceOfGaussian.comp.spv", &dog_shader_module))
    {
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian shader module");
      return false;
    }
    if (!VulkanUtils::createComputePipeline(m_device, dog_shader_module, m_dog_desc_set_layout, 0, &m_dog_pipeline_layout, &m_dog_pipeline))
    {
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian pipeline");
      vkDestroyShaderModule(m_device, dog_shader_module, nullptr);
      return false;
    }
    vkDestroyShaderModule(m_device, dog_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup ExtractKeypoints pipeline
  //////////////////////////////////////
  {
    VkShaderModule extractkpts_shader_module;
    if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/ExtractKeypoints.comp.spv", &extractkpts_shader_module))
    {
      logError(LOG_TAG, "Failed to create ExtractKeypoints shader module");
      return false;
    }
    if (!VulkanUtils::createComputePipeline(m_device, extractkpts_shader_module, m_extractkpts_desc_set_layout, sizeof(ExtractKeypointsPushConsts),
                                            &m_extractkpts_pipeline_layout, &m_extractkpts_pipeline))
    {
      logError(LOG_TAG, "Failed to create ExtractKeypoints pipeline");
      vkDestroyShaderModule(m_device, extractkpts_shader_module, nullptr);
      return false;
    }
    vkDestroyShaderModule(m_device, extractkpts_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup ComputeOrientation pipeline
  //////////////////////////////////////
  {
    VkShaderModule orientation_shader_module;
    if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/ComputeOrientation.comp.spv", &orientation_shader_module))
    {
      logError(LOG_TAG, "Failed to create ComputeOrientation shader module");
      return false;
    }
    if (!VulkanUtils::createComputePipeline(m_device, orientation_shader_module, m_orientation_desc_set_layout, 0, &m_orientation_pipeline_layout,
                                            &m_orientation_pipeline))
    {
      logError(LOG_TAG, "Failed to create ComputeOrientation pipeline");
      vkDestroyShaderModule(m_device, orientation_shader_module, nullptr);
      return false;
    }
    vkDestroyShaderModule(m_device, orientation_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup ComputeDescriptors pipeline
  //////////////////////////////////////
  {
    VkShaderModule descriptor_shader_module;
    if (!VulkanUtils::Shader::createShaderModule(m_device, "shaders/ComputeDescriptors.comp.spv", &descriptor_shader_module))
    {
      logError(LOG_TAG, "Failed to create ComputeDescriptors shader module");
      return false;
    }
    if (!VulkanUtils::createComputePipeline(m_device, descriptor_shader_module, m_descriptor_desc_set_layout, 0, &m_descriptor_pipeline_layout,
                                            &m_descriptor_pipeline))
    {
      logError(LOG_TAG, "Failed to create ComputeDescriptors pipeline");
      vkDestroyShaderModule(m_device, descriptor_shader_module, nullptr);
      return false;
    }
    vkDestroyShaderModule(m_device, descriptor_shader_module, nullptr);
  }
  return true;
}

void SiftDetector::beginMarkerRegion(VkCommandBuffer cmd_buf, const char *region_name)
{
  if (m_debug_marker_supported)
  {
    VkDebugMarkerMarkerInfoEXT marker_info{.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, .pMarkerName = region_name};
    m_vulkan_instance->vkCmdDebugMarkerBeginEXT(cmd_buf, &marker_info);
  }
}
void SiftDetector::endMarkerRegion(VkCommandBuffer cmd_buf)
{
  if (m_debug_marker_supported)
  {
    m_vulkan_instance->vkCmdDebugMarkerEndEXT(cmd_buf);
  }
}

bool SiftDetector::initCommandBuffer()
{
  VkCommandBufferAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                         .commandPool = m_command_pool,
                                         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                         .commandBufferCount = (uint32_t)1};

  if (vkAllocateCommandBuffers(m_device, &alloc_info, &m_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate command buffers");
    return false;
  }

  VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0, .pInheritanceInfo = nullptr};

  if (vkBeginCommandBuffer(m_command_buffer, &begin_info) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to begin recording command buffer");
    return false;
  }

  // Clear data
  beginMarkerRegion(m_command_buffer, "Clear data");
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    // Only reset the indirect dispatch buffers and the detected SIFT feature counter
    vkCmdFillBuffer(m_command_buffer, m_sift_keypoints_buffers[i].getBuffer(), 0, sizeof(uint32_t), 0);
    vkCmdFillBuffer(m_command_buffer, m_indispatch_orientation_buffers[i].getBuffer(), 0, VK_WHOLE_SIZE, 1);
    vkCmdFillBuffer(m_command_buffer, m_indispatch_orientation_buffers[i].getBuffer(), 0, sizeof(uint32_t), 0);
  }
  endMarkerRegion(m_command_buffer);

  // Copy input image
  beginMarkerRegion(m_command_buffer, "CopyInputImage");
  {
    std::vector<VkImageMemoryBarrier> image_barriers;
    image_barriers.push_back(m_input_image.getImageMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                         image_barriers.size(), image_barriers.data());
  }
  VkBufferImageCopy buffer_image_region{.bufferOffset = 0,
                                        .bufferRowLength = 0,
                                        .bufferImageHeight = 0,
                                        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                                        .imageOffset = {.x = 0, .y = 0, .z = 0},
                                        .imageExtent = {.width = m_image_width, .height = m_image_height, .depth = 1}};
  vkCmdCopyBufferToImage(m_command_buffer, m_input_image_staging_in_buffer.getBuffer(), m_input_image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &buffer_image_region);
  {
    std::vector<VkImageMemoryBarrier> image_barriers;
    image_barriers.push_back(m_input_image.getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         image_barriers.size(), image_barriers.data());
  }
  endMarkerRegion(m_command_buffer);

  // Scale space construction
  beginMarkerRegion(m_command_buffer, "Scale space construction");
  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline);
  for (uint32_t oct_i = 0; oct_i < m_nb_octave; oct_i++)
  {
    if (oct_i == 0)
    {
      // Upscale input image to get (Octave 0,Scale 0)
      VkImageBlit region{.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                         .srcOffsets = {{0, 0, 0}, {(int32_t)m_image_width, (int32_t)m_image_height, 1}},
                         .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                         .dstOffsets = {{0, 0, 0}, {(int32_t)m_octave_image_sizes[oct_i].width, (int32_t)m_octave_image_sizes[oct_i].height, 1}}};
      vkCmdBlitImage(m_command_buffer, m_input_image.getImage(), VK_IMAGE_LAYOUT_GENERAL, m_octave_images[oct_i].getImage(), VK_IMAGE_LAYOUT_GENERAL, 1,
                     &region, VK_FILTER_LINEAR);

      // Blur the first scale
      {
        std::vector<VkImageMemoryBarrier> image_barriers;
        image_barriers.push_back(m_blur_temp_results[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
        image_barriers.push_back(m_octave_images[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
        vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             image_barriers.size(), image_barriers.data());
      }
      GaussianBlurPushConsts pc{.is_vertical = 0, .array_layer = 0, .kernel_size = (uint32_t)m_gaussian_kernels[0].size()};
      memcpy(pc.kernel, m_gaussian_kernels[0].data(), sizeof(float) * m_gaussian_kernels[0].size());
      vkCmdPushConstants(m_command_buffer, m_blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &pc);
      vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline_layout, 0, 1, &m_blur_h_desc_sets[oct_i], 0, nullptr);
      vkCmdDispatch(m_command_buffer, ceilf(static_cast<float>(m_octave_image_sizes[oct_i].width) / 8.f),
                    ceilf(static_cast<float>(m_octave_image_sizes[oct_i].height) / 8.f), 1);
      {
        std::vector<VkImageMemoryBarrier> image_barriers;
        image_barriers.push_back(m_blur_temp_results[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
        image_barriers.push_back(m_octave_images[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
        vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             image_barriers.size(), image_barriers.data());
      }
      pc.is_vertical = 1;
      vkCmdPushConstants(m_command_buffer, m_blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &pc);
      vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline_layout, 0, 1, &m_blur_v_desc_sets[oct_i], 0, nullptr);
      vkCmdDispatch(m_command_buffer, ceilf(static_cast<float>(m_octave_image_sizes[oct_i].width) / 8.f),
                    ceilf(static_cast<float>(m_octave_image_sizes[oct_i].height) / 8.f), 1);
    }
    else
    {
      // Downscale previous octave image to get (Octave i,Scale i)
      VkImageBlit region{.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = m_nb_scale_per_oct, .layerCount = 1},
                         .srcOffsets = {{0, 0, 0}, {(int32_t)m_octave_image_sizes[oct_i - 1].width, (int32_t)m_octave_image_sizes[oct_i - 1].height, 1}},
                         .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                         .dstOffsets = {{0, 0, 0}, {(int32_t)m_octave_image_sizes[oct_i].width, (int32_t)m_octave_image_sizes[oct_i].height, 1}}};
      vkCmdBlitImage(m_command_buffer, m_octave_images[oct_i - 1].getImage(), VK_IMAGE_LAYOUT_GENERAL, m_octave_images[oct_i].getImage(),
                     VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_NEAREST);
    }

    for (uint32_t scale_i = 1; scale_i < m_nb_scale_per_oct + 3; scale_i++)
    {
      // Gaussian blur from one scale to the other
      {
        std::vector<VkImageMemoryBarrier> image_barriers;
        image_barriers.push_back(m_blur_temp_results[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
        image_barriers.push_back(m_octave_images[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
        vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             image_barriers.size(), image_barriers.data());
      }
      GaussianBlurPushConsts pc{.is_vertical = 0, .array_layer = (scale_i - 1), .kernel_size = (uint32_t)m_gaussian_kernels[scale_i].size()};
      memcpy(pc.kernel, m_gaussian_kernels[scale_i].data(), sizeof(float) * m_gaussian_kernels[scale_i].size());
      vkCmdPushConstants(m_command_buffer, m_blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &pc);
      vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline_layout, 0, 1, &m_blur_h_desc_sets[oct_i], 0, nullptr);
      vkCmdDispatch(m_command_buffer, ceilf(static_cast<float>(m_octave_image_sizes[oct_i].width) / 8.f),
                    ceilf(static_cast<float>(m_octave_image_sizes[oct_i].height) / 8.f), 1);
      {
        std::vector<VkImageMemoryBarrier> image_barriers;
        image_barriers.push_back(m_blur_temp_results[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
        image_barriers.push_back(m_octave_images[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
        vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             image_barriers.size(), image_barriers.data());
      }
      pc.is_vertical = 1;
      pc.array_layer = scale_i;
      vkCmdPushConstants(m_command_buffer, m_blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &pc);
      vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline_layout, 0, 1, &m_blur_v_desc_sets[oct_i], 0, nullptr);
      vkCmdDispatch(m_command_buffer, ceilf(static_cast<float>(m_octave_image_sizes[oct_i].width) / 8.f),
                    ceilf(static_cast<float>(m_octave_image_sizes[oct_i].height) / 8.f), 1);
    }
  }
  endMarkerRegion(m_command_buffer);

  {
    std::vector<VkImageMemoryBarrier> image_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      image_barriers.push_back(m_octave_images[i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
      image_barriers.push_back(m_octave_DoG_images[i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         image_barriers.size(), image_barriers.data());
  }

  // DifferenceOfGaussian
  beginMarkerRegion(m_command_buffer, "DoG computation");
  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_dog_pipeline);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_dog_pipeline_layout, 0, 1, &m_dog_desc_sets[i], 0, nullptr);
    vkCmdDispatch(m_command_buffer, ceilf(static_cast<float>(m_octave_image_sizes[i].width) / 8.f),
                  ceilf(static_cast<float>(m_octave_image_sizes[i].height) / 8.f), m_nb_scale_per_oct + 2);
  }
  {
    std::vector<VkImageMemoryBarrier> image_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      image_barriers.push_back(m_octave_DoG_images[i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         image_barriers.size(), image_barriers.data());
  }
  endMarkerRegion(m_command_buffer);

  // ExtractKeypoints
  beginMarkerRegion(m_command_buffer, "ExtrackKeypoints");
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
      buffer_barriers.push_back(m_indispatch_orientation_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
  }

  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_extractkpts_pipeline);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    ExtractKeypointsPushConsts pushconst;
    pushconst.sigma_multiplier = powf(2.f, 1.f / m_nb_scale_per_oct) * m_sigma_min;
    pushconst.scale_factor = powf(2.f, i) * m_scale_factor_min;
    pushconst.dog_threshold = m_dog_threshold;
    pushconst.edge_threshold = m_kp_edge_threshold;
    // logError(LOG_TAG, "sigmul %f", pushconst.sigma_multiplier);
    vkCmdPushConstants(m_command_buffer, m_extractkpts_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ExtractKeypointsPushConsts), &pushconst);
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_extractkpts_pipeline_layout, 0, 1, &m_extractkpts_desc_sets[i], 0,
                            nullptr);
    vkCmdDispatch(m_command_buffer, ceilf(static_cast<float>(m_octave_image_sizes[i].width) / 8.f),
                  ceilf(static_cast<float>(m_octave_image_sizes[i].height) / 8.f), m_nb_scale_per_oct);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
  }
  endMarkerRegion(m_command_buffer);

  // Copy one indispatch buffer to the other
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_indispatch_orientation_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_READ_BIT));
      buffer_barriers.push_back(m_indispatch_descriptors_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = sizeof(uint32_t) * 3};
    vkCmdCopyBuffer(m_command_buffer, m_indispatch_orientation_buffers[i].getBuffer(), m_indispatch_descriptors_buffers[i].getBuffer(), 1, &region);
  }

  beginMarkerRegion(m_command_buffer, "ComputeOrientation");
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_indispatch_orientation_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_INDIRECT_COMMAND_READ_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_indispatch_descriptors_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_orientation_pipeline);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_orientation_pipeline_layout, 0, 1, &m_orientation_desc_sets[i], 0,
                            nullptr);
    vkCmdDispatchIndirect(m_command_buffer, m_indispatch_orientation_buffers[i].getBuffer(), 0);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
  }
  endMarkerRegion(m_command_buffer);

  beginMarkerRegion(m_command_buffer, "ComputeDescriptors");
  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_descriptor_pipeline);
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_indispatch_descriptors_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_INDIRECT_COMMAND_READ_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr,
                         buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
  }
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_descriptor_pipeline_layout, 0, 1, &m_descriptor_desc_sets[i], 0, nullptr);
    vkCmdDispatchIndirect(m_command_buffer, m_indispatch_descriptors_buffers[i].getBuffer(), 0);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
  }
  endMarkerRegion(m_command_buffer);

  beginMarkerRegion(m_command_buffer, "CopySiftHeader");
  {
    // Only copy the number of detected SIFT features to the staging buffer (accessible by host)
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_READ_BIT));
      buffer_barriers.push_back(m_sift_staging_out_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    VkBufferCopy sift_copy_region{.srcOffset = 0u, .dstOffset = 0u, .size = sizeof(uint32_t)};
    vkCmdCopyBuffer(m_command_buffer, m_sift_keypoints_buffers[i].getBuffer(), m_sift_staging_out_buffers[i].getBuffer(), 1, &sift_copy_region);
  }
  endMarkerRegion(m_command_buffer);

  if (vkEndCommandBuffer(m_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to record command buffer");
    return false;
  }
  return true;
}

bool SiftDetector::compute(uint8_t *pixel_buffer, std::vector<SIFT_Feature> &sift_feats)
{
  vkResetFences(m_device, 1, &m_fence);
  // Copy image
  m_input_image_staging_in_buffer.invalidateMappedMemory(m_device, 0, VK_WHOLE_SIZE);
  memcpy(m_input_image_ptr, pixel_buffer, m_image_width * m_image_height * sizeof(uint8_t));
  m_input_image_staging_in_buffer.flushMappedMemory(m_device, 0, VK_WHOLE_SIZE);

  {
    VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .waitSemaphoreCount = 0,
                             .commandBufferCount = 1,
                             .pCommandBuffers = &m_command_buffer,
                             .signalSemaphoreCount = 0};
    if (vkQueueSubmit(m_queue, 1, &submit_info, m_fence) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to submit command buffer");
      return false;
    }
  }

  vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, UINT64_MAX);

  // TODO: CPU get info on number of features detected, create a minimal GPU->STAGING copy command buffer, then copy on CPU

  // Copy SIFT features
  sift_feats.clear();

  int total_nb_sift = 0;
  std::vector<int> nb_feat_per_octave;

  // For each octave, read the number of detected features from the GPU memory
  // Only invalidate the first 4 bytes of the buffer (number of features detected) here since that's the only thing we read
  VkDeviceSize header_range_size =
      (sizeof(uint32_t) / m_physical_device_props.limits.nonCoherentAtomSize + 1) * m_physical_device_props.limits.nonCoherentAtomSize;
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    m_sift_staging_out_buffers[i].invalidateMappedMemory(m_device, 0, header_range_size);

    uint32_t nb_sift_feat = ((uint32_t *)m_output_sift_ptr[i])[0];
    nb_sift_feat = std::min(nb_sift_feat, m_max_nb_feat_per_octave[i]);
    nb_feat_per_octave.push_back(nb_sift_feat);
    total_nb_sift += nb_sift_feat;
  }

  sift_feats.resize(total_nb_sift);

  // Copy only the right number of features to the GPU staging buffer
  bool copy_cmd_res = VulkanUtils::submitCommandsAndWait(m_device, m_queue, m_command_pool, [&](VkCommandBuffer cmdbuf) {
    beginMarkerRegion(cmdbuf, "CopySiftFeats");
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      VkBufferCopy sift_copy_region{.srcOffset = 0u, .dstOffset = 0u, .size = sizeof(SIFT_Feature) * nb_feat_per_octave[i] + sizeof(uint32_t)};
      vkCmdCopyBuffer(cmdbuf, m_sift_keypoints_buffers[i].getBuffer(), m_sift_staging_out_buffers[i].getBuffer(), 1, &sift_copy_region);
    }
    endMarkerRegion(cmdbuf);
  });
  if (!copy_cmd_res)
  {
    logError(LOG_TAG, "Failed to copy SIFT features");
    return false;
  }

  // Read the SIFT features from the staging buffer
  int vec_offset = 0;
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    // Only invalidate what we are going to read (possible since we know exactly the number of feature in each buffer)
    VkDeviceSize min_range_size =
        ((sizeof(SIFT_Feature) * nb_feat_per_octave[i] + sizeof(uint32_t)) / m_physical_device_props.limits.nonCoherentAtomSize + 1) *
        m_physical_device_props.limits.nonCoherentAtomSize;
    m_sift_staging_out_buffers[i].invalidateMappedMemory(m_device, 0, min_range_size);
    SIFT_Feature *sift_feats_ptr = (SIFT_Feature *)((uint32_t *)(m_output_sift_ptr[i]) + 1);
    memcpy(sift_feats.data() + vec_offset, sift_feats_ptr, sizeof(SIFT_Feature) * nb_feat_per_octave[i]);
    vec_offset += nb_feat_per_octave[i];
  }

  // logInfo(LOG_TAG, "%d SIFT found", int(sift_feats.size()));
  // logInfo(LOG_TAG, "%d SIFT found", total_nb_sift);
  return true;
}

void SiftDetector::terminate()
{
  vkQueueWaitIdle(m_queue);

  // Destroy sync objects and command buffers

  VK_NULL_SAFE_DELETE(m_fence, vkDestroyFence(m_device, m_fence, nullptr));
  VK_NULL_SAFE_DELETE(m_command_buffer, vkFreeCommandBuffers(m_device, m_command_pool, 1, &m_command_buffer));

  // Destroy pipelines and descriptors
  // GaussianBlur
  VK_NULL_SAFE_DELETE(m_blur_pipeline, vkDestroyPipeline(m_device, m_blur_pipeline, nullptr));
  VK_NULL_SAFE_DELETE(m_blur_pipeline_layout, vkDestroyPipelineLayout(m_device, m_blur_pipeline_layout, nullptr));
  VK_NULL_SAFE_DELETE(m_blur_desc_pool, vkDestroyDescriptorPool(m_device, m_blur_desc_pool, nullptr));
  VK_NULL_SAFE_DELETE(m_blur_desc_set_layout, vkDestroyDescriptorSetLayout(m_device, m_blur_desc_set_layout, nullptr));
  // DifferenceOfGaussian
  VK_NULL_SAFE_DELETE(m_dog_pipeline, vkDestroyPipeline(m_device, m_dog_pipeline, nullptr));
  VK_NULL_SAFE_DELETE(m_dog_pipeline_layout, vkDestroyPipelineLayout(m_device, m_dog_pipeline_layout, nullptr));
  VK_NULL_SAFE_DELETE(m_dog_desc_pool, vkDestroyDescriptorPool(m_device, m_dog_desc_pool, nullptr));
  VK_NULL_SAFE_DELETE(m_dog_desc_set_layout, vkDestroyDescriptorSetLayout(m_device, m_dog_desc_set_layout, nullptr));
  // ExtractKeypoints
  VK_NULL_SAFE_DELETE(m_extractkpts_pipeline, vkDestroyPipeline(m_device, m_extractkpts_pipeline, nullptr));
  VK_NULL_SAFE_DELETE(m_extractkpts_pipeline_layout, vkDestroyPipelineLayout(m_device, m_extractkpts_pipeline_layout, nullptr));
  VK_NULL_SAFE_DELETE(m_extractkpts_desc_pool, vkDestroyDescriptorPool(m_device, m_extractkpts_desc_pool, nullptr));
  VK_NULL_SAFE_DELETE(m_extractkpts_desc_set_layout, vkDestroyDescriptorSetLayout(m_device, m_extractkpts_desc_set_layout, nullptr));
  // ComputeOrientation
  VK_NULL_SAFE_DELETE(m_orientation_pipeline, vkDestroyPipeline(m_device, m_orientation_pipeline, nullptr));
  VK_NULL_SAFE_DELETE(m_orientation_pipeline_layout, vkDestroyPipelineLayout(m_device, m_orientation_pipeline_layout, nullptr));
  VK_NULL_SAFE_DELETE(m_orientation_desc_pool, vkDestroyDescriptorPool(m_device, m_orientation_desc_pool, nullptr));
  VK_NULL_SAFE_DELETE(m_orientation_desc_set_layout, vkDestroyDescriptorSetLayout(m_device, m_orientation_desc_set_layout, nullptr));
  // ComputeDescriptor
  VK_NULL_SAFE_DELETE(m_descriptor_pipeline, vkDestroyPipeline(m_device, m_descriptor_pipeline, nullptr));
  VK_NULL_SAFE_DELETE(m_descriptor_pipeline_layout, vkDestroyPipelineLayout(m_device, m_descriptor_pipeline_layout, nullptr));
  VK_NULL_SAFE_DELETE(m_descriptor_desc_pool, vkDestroyDescriptorPool(m_device, m_descriptor_desc_pool, nullptr));
  VK_NULL_SAFE_DELETE(m_descriptor_desc_set_layout, vkDestroyDescriptorSetLayout(m_device, m_descriptor_desc_set_layout, nullptr));

  // Destroy sampler
  VK_NULL_SAFE_DELETE(m_sampler, vkDestroySampler(m_device, m_sampler, nullptr));

  // Destroy memory objects
  // Memory is automatically unmapped if needed in the destroy function
  m_input_image.destroy(m_device);
  m_input_image_staging_in_buffer.destroy(m_device);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    m_blur_temp_results[i].destroy(m_device);
    m_octave_images[i].destroy(m_device);
    m_octave_DoG_images[i].destroy(m_device);
    m_indispatch_orientation_buffers[i].destroy(m_device);
    m_indispatch_descriptors_buffers[i].destroy(m_device);
    m_sift_keypoints_buffers[i].destroy(m_device);
    m_sift_staging_out_buffers[i].destroy(m_device);
  }
  m_octave_images.clear();
  m_blur_temp_results.clear();
  m_octave_DoG_images.clear();
  m_indispatch_orientation_buffers.clear();
  m_indispatch_descriptors_buffers.clear();
  m_sift_keypoints_buffers.clear();
  m_sift_staging_out_buffers.clear();

  // Destroy command pool
  VK_NULL_SAFE_DELETE(m_command_pool, vkDestroyCommandPool(m_device, m_command_pool, nullptr));

  // Reset to VK_NULL_HANDLE object obtained from VulkanManager
  m_vulkan_instance = nullptr;
  m_physical_device = VK_NULL_HANDLE;
  m_device = VK_NULL_HANDLE;
  m_queue = VK_NULL_HANDLE;
  m_queue_family_index = 0u;
  m_async_queue = VK_NULL_HANDLE;
  m_async_queue_family_index = 0u;
}

} // namespace VulkanSIFT