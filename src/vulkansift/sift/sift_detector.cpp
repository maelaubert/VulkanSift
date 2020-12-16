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

bool SiftDetector::init(VulkanInstance *vulkan_instance, const int image_width, const int image_height)
{
  m_image_width = image_width;
  m_image_height = image_height;

  // Compute image sizes (per octave and scale)
  for (uint32_t oct_i = 0; oct_i < m_nb_octave; oct_i++)
  {
    uint32_t octave_width = (1.f / (powf(2.f, oct_i) * m_scale_factor_min)) * static_cast<float>(m_image_width);
    uint32_t octave_height = (1.f / (powf(2.f, oct_i) * m_scale_factor_min)) * static_cast<float>(m_image_height);
    m_octave_image_sizes.push_back({octave_width, octave_height});
  }

  m_vulkan_instance = vulkan_instance;

  // Get Vulkan entities and information from VulkanManager
  m_physical_device = m_vulkan_instance->getVkPhysicalDevice();
  if (m_physical_device == VK_NULL_HANDLE)
  {
    logError(LOG_TAG, "VulkanManager returned a NULL physical device.");
    return false;
  }
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
      if (!m_blur_temp_results[i].create(m_device, m_physical_device, m_octave_image_sizes[i].width, m_octave_image_sizes[i].height, VK_FORMAT_R16_UNORM,
                                         VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
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
      if (!m_octave_images[i].create(m_device, m_physical_device, m_octave_image_sizes[i].width, m_octave_image_sizes[i].height * (m_nb_scale_per_oct + 3),
                                     VK_FORMAT_R16_SNORM, VK_IMAGE_TILING_OPTIMAL,
                                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
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
      if (!m_octave_DoG_images[i].create(m_device, m_physical_device, m_octave_image_sizes[i].width,
                                         m_octave_image_sizes[i].height * (m_nb_scale_per_oct + 2), VK_FORMAT_R16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                                         // m_octave_image_sizes[i].height * (m_nb_scale_per_oct + 2), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                                         VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        logError(LOG_TAG, "Failed to create DoG octave image.");
        return false;
      }
    }
  }

  // Need IndirectDispatch buffers info for orientation and descriptor dispatch
  {
    m_indispatch_buffers.resize(m_nb_octave);
    VkDeviceSize buffer_size = sizeof(uint32_t) * 3;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_indispatch_buffers[i].create(m_device, m_physical_device, buffer_size,
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        logError(LOG_TAG, "Failed to create indirect dispatch buffer.");
        return false;
      }
    }
  }

  // Create buffer to store SIFT data
  {
    m_sift_keypoints_buffers.resize(m_nb_octave);
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      if (!m_sift_keypoints_buffers[i].create(m_device, m_physical_device, sizeof(uint32_t) + (m_sift_buff_max_elem * sizeof(SIFT_Feature)),
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
      if (!m_sift_staging_out_buffers[i].create(m_device, m_physical_device, sizeof(uint32_t) + (m_sift_buff_max_elem * sizeof(SIFT_Feature)),
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

  if (vkMapMemory(m_device, m_input_image_staging_in_buffer.getBufferMemory(), 0, m_image_width * m_image_height * sizeof(uint8_t), 0,
                  &m_input_image_ptr) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to map input buffer memory.");
    return false;
  }

  m_output_sift_ptr.resize(m_nb_octave);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    if (vkMapMemory(m_device, m_sift_staging_out_buffers[i].getBufferMemory(), 0, VK_WHOLE_SIZE, 0, &m_output_sift_ptr[i]) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to map output buffer memory.");
      return false;
    }
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
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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
    pool_sizes[0] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = m_nb_octave * 2};
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
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorImageInfo blur_work_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_blur_temp_results[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorImageInfo blur_output_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      std::array<VkWriteDescriptorSet, 2> descriptor_writes;
      // First write for horizontal pass
      descriptor_writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                              .dstSet = m_blur_h_desc_sets[i],
                              .dstBinding = 0,
                              .dstArrayElement = 0,
                              .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian descriptor set layout");
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
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian descriptor pool");
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
      logError(LOG_TAG, "Failed to allocate DifferenceOfGaussian descriptor set");
      return false;
    }

    // Write descriptor sets
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      VkDescriptorImageInfo dog_input_image_info{
          .sampler = VK_NULL_HANDLE, .imageView = m_octave_DoG_images[i].getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
      VkDescriptorBufferInfo sift_buffer_info{.buffer = m_sift_keypoints_buffers[i].getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo indispatch_buffer_info{.buffer = m_indispatch_buffers[i].getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
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

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{octave_image_layout_binding, sift_buffer_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_orientation_desc_set_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeOrientation descriptor set layout");
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
      std::array<VkWriteDescriptorSet, 2> descriptor_writes;
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
  float sigma;
  uint32_t offset_y;
};

struct ExtractKeypointsPushConsts
{
  uint32_t offset_y_per_scale;
  float scale_factor;
  float sigma_multiplier;
  float soft_dog_threshold;
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
    VulkanUtils::Shader::createShaderModule(m_device, "shaders/GaussianBlur.comp.spv", &blur_shader_module);

    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0u, .size = sizeof(uint32_t) + sizeof(float) + sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo blur_pipeline_layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         .setLayoutCount = 1,
                                                         .pSetLayouts = &m_blur_desc_set_layout,
                                                         .pushConstantRangeCount = 1,
                                                         .pPushConstantRanges = &push_constant_range};
    if (vkCreatePipelineLayout(m_device, &blur_pipeline_layout_info, nullptr, &m_blur_pipeline_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create GaussianBlur pipeline layout");
      return false;
    }

    VkPipelineShaderStageCreateInfo blur_pipeline_shader_stage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = blur_shader_module, .pName = "main"};

    VkComputePipelineCreateInfo blur_pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                      .pNext = nullptr,
                                                      .flags = 0,
                                                      .stage = blur_pipeline_shader_stage,
                                                      .layout = m_blur_pipeline_layout,
                                                      .basePipelineHandle = VK_NULL_HANDLE,
                                                      .basePipelineIndex = -1};
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &blur_pipeline_info, nullptr, &m_blur_pipeline) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create GaussianBlur pipeline");
      return false;
    }
    vkDestroyShaderModule(m_device, blur_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup DifferenceOfGaussian pipeline
  //////////////////////////////////////
  {
    VkShaderModule dog_shader_module;
    VulkanUtils::Shader::createShaderModule(m_device, "shaders/DifferenceOfGaussian.comp.spv", &dog_shader_module);

    VkPushConstantRange push_constant_range{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0u, .size = sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo dog_pipeline_layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                        .setLayoutCount = 1,
                                                        .pSetLayouts = &m_dog_desc_set_layout,
                                                        .pushConstantRangeCount = 1,
                                                        .pPushConstantRanges = &push_constant_range};
    if (vkCreatePipelineLayout(m_device, &dog_pipeline_layout_info, nullptr, &m_dog_pipeline_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian pipeline layout");
      return false;
    }

    VkPipelineShaderStageCreateInfo dog_pipeline_shader_stage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = dog_shader_module, .pName = "main"};

    VkComputePipelineCreateInfo dog_pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .stage = dog_pipeline_shader_stage,
                                                     .layout = m_dog_pipeline_layout,
                                                     .basePipelineHandle = VK_NULL_HANDLE,
                                                     .basePipelineIndex = -1};
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &dog_pipeline_info, nullptr, &m_dog_pipeline) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create DifferenceOfGaussian pipeline");
      return false;
    }
    vkDestroyShaderModule(m_device, dog_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup ExtractKeypoints pipeline
  //////////////////////////////////////
  {
    VkShaderModule extractkpts_shader_module;
    VulkanUtils::Shader::createShaderModule(m_device, "shaders/ExtractKeypoints.comp.spv", &extractkpts_shader_module);

    VkPushConstantRange push_constant_range{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0u, .size = sizeof(ExtractKeypointsPushConsts)};
    VkPipelineLayoutCreateInfo extractkpts_pipeline_layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                                .setLayoutCount = 1,
                                                                .pSetLayouts = &m_extractkpts_desc_set_layout,
                                                                .pushConstantRangeCount = 1,
                                                                .pPushConstantRanges = &push_constant_range};
    if (vkCreatePipelineLayout(m_device, &extractkpts_pipeline_layout_info, nullptr, &m_extractkpts_pipeline_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ExtractKeypoints pipeline layout");
      return false;
    }

    VkPipelineShaderStageCreateInfo extractkpts_pipeline_shader_stage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                      .module = extractkpts_shader_module,
                                                                      .pName = "main"};

    VkComputePipelineCreateInfo extractkpts_pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                             .pNext = nullptr,
                                                             .flags = 0,
                                                             .stage = extractkpts_pipeline_shader_stage,
                                                             .layout = m_extractkpts_pipeline_layout,
                                                             .basePipelineHandle = VK_NULL_HANDLE,
                                                             .basePipelineIndex = -1};
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &extractkpts_pipeline_info, nullptr, &m_extractkpts_pipeline) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ExtractKeypoints pipeline");
      return false;
    }
    vkDestroyShaderModule(m_device, extractkpts_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup ComputeOrientation pipeline
  //////////////////////////////////////
  {
    VkShaderModule orientation_shader_module;
    VulkanUtils::Shader::createShaderModule(m_device, "shaders/ComputeOrientation.comp.spv", &orientation_shader_module);

    VkPushConstantRange push_constant_range{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0u, .size = sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo orientation_pipeline_layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                                .setLayoutCount = 1,
                                                                .pSetLayouts = &m_orientation_desc_set_layout,
                                                                .pushConstantRangeCount = 1,
                                                                .pPushConstantRanges = &push_constant_range};
    if (vkCreatePipelineLayout(m_device, &orientation_pipeline_layout_info, nullptr, &m_orientation_pipeline_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeOrientation pipeline layout");
      return false;
    }

    VkPipelineShaderStageCreateInfo orientation_pipeline_shader_stage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                      .module = orientation_shader_module,
                                                                      .pName = "main"};

    VkComputePipelineCreateInfo orientation_pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                             .pNext = nullptr,
                                                             .flags = 0,
                                                             .stage = orientation_pipeline_shader_stage,
                                                             .layout = m_orientation_pipeline_layout,
                                                             .basePipelineHandle = VK_NULL_HANDLE,
                                                             .basePipelineIndex = -1};
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &orientation_pipeline_info, nullptr, &m_orientation_pipeline) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeOrientation pipeline");
      return false;
    }
    vkDestroyShaderModule(m_device, orientation_shader_module, nullptr);
  }
  //////////////////////////////////////
  // Setup ComputeDescriptors pipeline
  //////////////////////////////////////
  {
    VkShaderModule descriptor_shader_module;
    VulkanUtils::Shader::createShaderModule(m_device, "shaders/ComputeDescriptors.comp.spv", &descriptor_shader_module);

    VkPushConstantRange push_constant_range{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0u, .size = sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo descriptor_pipeline_layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                               .setLayoutCount = 1,
                                                               .pSetLayouts = &m_descriptor_desc_set_layout,
                                                               .pushConstantRangeCount = 1,
                                                               .pPushConstantRanges = &push_constant_range};
    if (vkCreatePipelineLayout(m_device, &descriptor_pipeline_layout_info, nullptr, &m_descriptor_pipeline_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeDescriptors pipeline layout");
      return false;
    }

    VkPipelineShaderStageCreateInfo descriptor_pipeline_shader_stage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                     .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                     .module = descriptor_shader_module,
                                                                     .pName = "main"};

    VkComputePipelineCreateInfo descriptor_pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                            .pNext = nullptr,
                                                            .flags = 0,
                                                            .stage = descriptor_pipeline_shader_stage,
                                                            .layout = m_descriptor_pipeline_layout,
                                                            .basePipelineHandle = VK_NULL_HANDLE,
                                                            .basePipelineIndex = -1};
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &descriptor_pipeline_info, nullptr, &m_descriptor_pipeline) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create ComputeDescriptors pipeline");
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
  VkClearColorValue clear_color{{0.0, 0.0, 0.0, 0.0}};
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(m_command_buffer, m_octave_images[i].getImage(), VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &range);
    vkCmdClearColorImage(m_command_buffer, m_blur_temp_results[i].getImage(), VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &range);
    vkCmdFillBuffer(m_command_buffer, m_sift_keypoints_buffers[i].getBuffer(), 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(m_command_buffer, m_indispatch_buffers[i].getBuffer(), 0, VK_WHOLE_SIZE, 1);
    vkCmdFillBuffer(m_command_buffer, m_indispatch_buffers[i].getBuffer(), 0, sizeof(uint32_t), 0);
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
      float sep_kernel_sigma = sqrtf((m_sigma_min * m_sigma_min) - (m_sigma_in * m_sigma_in)) / m_scale_factor_min;

      {
        std::vector<VkImageMemoryBarrier> image_barriers;
        image_barriers.push_back(m_blur_temp_results[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
        image_barriers.push_back(m_octave_images[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
        vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             image_barriers.size(), image_barriers.data());
      }
      GaussianBlurPushConsts pc{0, sep_kernel_sigma, 0};
      vkCmdPushConstants(m_command_buffer, m_blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &pc);
      vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline_layout, 0, 1, &m_blur_h_desc_sets[oct_i], 0, nullptr);
      vkCmdDispatch(m_command_buffer, m_octave_image_sizes[oct_i].width / 8, m_octave_image_sizes[oct_i].height / 8, 1);
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
      vkCmdDispatch(m_command_buffer, m_octave_image_sizes[oct_i].width / 8, m_octave_image_sizes[oct_i].height / 8, 1);

      int kernel_size = static_cast<int>(ceilf(sep_kernel_sigma * 4.f) + 1.f);
      logError(LOG_TAG, "Kernel size %d", kernel_size);
    }
    else
    {
      // Downscale previous octave image to get (Octave i,Scale i)
      VkImageBlit region{.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                         .srcOffsets = {{0, (int32_t)(m_octave_image_sizes[oct_i - 1].height * m_nb_scale_per_oct), 0},
                                        {(int32_t)m_octave_image_sizes[oct_i - 1].width,
                                         (int32_t)(m_octave_image_sizes[oct_i - 1].height * (m_nb_scale_per_oct + 1)), 1}},
                         .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                         .dstOffsets = {{0, 0, 0}, {(int32_t)m_octave_image_sizes[oct_i].width, (int32_t)m_octave_image_sizes[oct_i].height, 1}}};
      vkCmdBlitImage(m_command_buffer, m_octave_images[oct_i - 1].getImage(), VK_IMAGE_LAYOUT_GENERAL, m_octave_images[oct_i].getImage(),
                     VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_LINEAR);
    }

    for (uint32_t scale_i = 1; scale_i < m_nb_scale_per_oct + 3; scale_i++)
    {
      // Gaussian blur from one scale to the other
      float prev_sigma =
          powf(2.f, static_cast<float>(oct_i)) * m_sigma_min * powf(2.f, static_cast<float>(scale_i - 1) / static_cast<float>(m_nb_scale_per_oct));
      float curr_sigma =
          powf(2.f, static_cast<float>(oct_i)) * m_sigma_min * powf(2.f, static_cast<float>(scale_i) / static_cast<float>(m_nb_scale_per_oct));
      float sep_kernel_sigma = sqrtf((curr_sigma * curr_sigma) - (prev_sigma * prev_sigma)) / (powf(2.f, static_cast<float>(oct_i)) * m_scale_factor_min);
      int kernel_size = static_cast<int>(ceilf(sep_kernel_sigma * 4.f) + 1.f);
      logError(LOG_TAG, "Kernel size %d", kernel_size);

      {
        std::vector<VkImageMemoryBarrier> image_barriers;
        image_barriers.push_back(m_blur_temp_results[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
        image_barriers.push_back(m_octave_images[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
        vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             image_barriers.size(), image_barriers.data());
      }
      GaussianBlurPushConsts pc{0, sep_kernel_sigma, (scale_i - 1) * m_octave_image_sizes[oct_i].height};
      vkCmdPushConstants(m_command_buffer, m_blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &pc);
      vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline_layout, 0, 1, &m_blur_h_desc_sets[oct_i], 0, nullptr);
      vkCmdDispatch(m_command_buffer, m_octave_image_sizes[oct_i].width / 8, m_octave_image_sizes[oct_i].height / 8, 1);
      {
        std::vector<VkImageMemoryBarrier> image_barriers;
        image_barriers.push_back(m_blur_temp_results[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL));
        image_barriers.push_back(m_octave_images[oct_i].getImageMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));
        vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             image_barriers.size(), image_barriers.data());
      }
      pc.is_vertical = 1;
      pc.offset_y = scale_i * m_octave_image_sizes[oct_i].height;
      vkCmdPushConstants(m_command_buffer, m_blur_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GaussianBlurPushConsts), &pc);
      vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline_layout, 0, 1, &m_blur_v_desc_sets[oct_i], 0, nullptr);
      vkCmdDispatch(m_command_buffer, m_octave_image_sizes[oct_i].width / 8, m_octave_image_sizes[oct_i].height / 8, 1);
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
    uint32_t y_offset = m_octave_image_sizes[i].height;
    vkCmdPushConstants(m_command_buffer, m_dog_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &y_offset);
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_dog_pipeline_layout, 0, 1, &m_dog_desc_sets[i], 0, nullptr);
    vkCmdDispatch(m_command_buffer, m_octave_image_sizes[i].width / 8, (m_octave_image_sizes[i].height * (m_nb_scale_per_oct + 2)) / 8, 1);
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
      buffer_barriers.push_back(m_indispatch_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
  }

  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_extractkpts_pipeline);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    ExtractKeypointsPushConsts pushconst;
    pushconst.offset_y_per_scale = m_octave_image_sizes[i].height;
    pushconst.sigma_multiplier = powf(2.f, static_cast<float>(i)) * m_sigma_min;
    pushconst.scale_factor = powf(2.f, i) * m_scale_factor_min;
    pushconst.soft_dog_threshold = m_soft_dog_threshold;
    pushconst.dog_threshold = m_dog_threshold;
    pushconst.edge_threshold = m_kp_edge_threshold;
    logError(LOG_TAG, "sigmul %f", pushconst.sigma_multiplier);
    vkCmdPushConstants(m_command_buffer, m_extractkpts_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ExtractKeypointsPushConsts), &pushconst);
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_extractkpts_pipeline_layout, 0, 1, &m_extractkpts_desc_sets[i], 0,
                            nullptr);
    vkCmdDispatch(m_command_buffer, m_octave_image_sizes[i].width / 8, m_octave_image_sizes[i].height / 8, m_nb_scale_per_oct);
    {
      std::vector<VkBufferMemoryBarrier> buffer_barriers;
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
      vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
    }
  }
  endMarkerRegion(m_command_buffer);

  beginMarkerRegion(m_command_buffer, "ComputeOrientation");
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_indispatch_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_INDIRECT_COMMAND_READ_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr,
                         buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
  }
  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_orientation_pipeline);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    uint pushconst = m_octave_image_sizes[i].height;
    vkCmdPushConstants(m_command_buffer, m_orientation_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &pushconst);
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_orientation_pipeline_layout, 0, 1, &m_orientation_desc_sets[i], 0,
                            nullptr);
    vkCmdDispatchIndirect(m_command_buffer, m_indispatch_buffers[i].getBuffer(), 0);
    {
      std::vector<VkBufferMemoryBarrier> buffer_barriers;
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
      vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
    }
  }
  endMarkerRegion(m_command_buffer);

  beginMarkerRegion(m_command_buffer, "ComputeDescriptors");
  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_descriptor_pipeline);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    uint32_t pushconst = m_octave_image_sizes[i].height;
    vkCmdPushConstants(m_command_buffer, m_descriptor_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &pushconst);
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_descriptor_pipeline_layout, 0, 1, &m_descriptor_desc_sets[i], 0, nullptr);
    vkCmdDispatchIndirect(m_command_buffer, m_indispatch_buffers[i].getBuffer(), 0);
    {
      std::vector<VkBufferMemoryBarrier> buffer_barriers;
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_WRITE_BIT));
      vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           buffer_barriers.size(), buffer_barriers.data(), 0, nullptr);
    }
  }
  endMarkerRegion(m_command_buffer);

  beginMarkerRegion(m_command_buffer, "CopySiftFeats");
  {
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
    VkBufferCopy orb_copy_region{.srcOffset = 0u, .dstOffset = 0u, .size = sizeof(SIFT_Feature) * m_sift_buff_max_elem + sizeof(uint32_t)};
    vkCmdCopyBuffer(m_command_buffer, m_sift_keypoints_buffers[i].getBuffer(), m_sift_staging_out_buffers[i].getBuffer(), 1, &orb_copy_region);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_sift_keypoints_buffers[i].getBufferMemoryBarrierAndUpdate(0));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    for (uint32_t i = 0; i < m_nb_octave; i++)
    {
      buffer_barriers.push_back(m_sift_staging_out_buffers[i].getBufferMemoryBarrierAndUpdate(VK_ACCESS_HOST_READ_BIT));
    }
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
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
  VkMappedMemoryRange mem_range{
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m_input_image_staging_in_buffer.getBufferMemory(), .offset = 0u, .size = VK_WHOLE_SIZE};
  vkInvalidateMappedMemoryRanges(m_device, 1, &mem_range);
  memcpy(m_input_image_ptr, pixel_buffer, m_image_width * m_image_height * sizeof(uint8_t));
  vkFlushMappedMemoryRanges(m_device, 1, &mem_range);

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

  // Copy SIFT features
  logInfo(LOG_TAG, "Before SIFT copy");
  sift_feats.clear();

  int total_nb_sift = 0;
  std::vector<int> nb_feat_per_octave;

  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    VkMappedMemoryRange mem_range_output{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m_sift_staging_out_buffers[i].getBufferMemory(), .offset = 0u, .size = VK_WHOLE_SIZE};
    vkInvalidateMappedMemoryRanges(m_device, 1, &mem_range_output);
    uint32_t nb_sift_feat = ((uint32_t *)m_output_sift_ptr[i])[0];
    nb_sift_feat = std::min(nb_sift_feat, m_sift_buff_max_elem);
    nb_feat_per_octave.push_back(nb_sift_feat);
    total_nb_sift += nb_sift_feat;
  }
  sift_feats.resize(total_nb_sift);

  int vec_offset = 0;
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    SIFT_Feature *orb_feats_ptr = (SIFT_Feature *)((uint32_t *)(m_output_sift_ptr[i]) + 1);
    memcpy(sift_feats.data() + vec_offset, orb_feats_ptr, sizeof(SIFT_Feature) * nb_feat_per_octave[i]);
    vec_offset += nb_feat_per_octave[i];
  }

  logInfo(LOG_TAG, "%d SIFT found", int(sift_feats.size()));
  logInfo(LOG_TAG, "%d SIFT found", total_nb_sift);
  return true;
}

void SiftDetector::terminate()
{
  vkQueueWaitIdle(m_queue);

  // Destroy sync objects and command buffers

  VK_NULL_SAFE_DELETE(m_fence, vkDestroyFence(m_device, m_fence, nullptr));
  vkFreeCommandBuffers(m_device, m_command_pool, 1, &m_command_buffer);

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

  // Destroy memory objects
  // Unmap before
  vkUnmapMemory(m_device, m_input_image_staging_in_buffer.getBufferMemory());
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    vkUnmapMemory(m_device, m_sift_staging_out_buffers[i].getBufferMemory());
  }

  m_input_image.destroy(m_device);
  m_input_image_staging_in_buffer.destroy(m_device);
  for (uint32_t i = 0; i < m_nb_octave; i++)
  {
    m_blur_temp_results[i].destroy(m_device);
    m_octave_images[i].destroy(m_device);
    m_octave_DoG_images[i].destroy(m_device);
    m_indispatch_buffers[i].destroy(m_device);
    m_sift_keypoints_buffers[i].destroy(m_device);
    m_sift_staging_out_buffers[i].destroy(m_device);
  }
  m_octave_images.clear();
  m_blur_temp_results.clear();
  m_octave_DoG_images.clear();
  m_indispatch_buffers.clear();
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