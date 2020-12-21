#include "vulkansift/sift/sift_matcher.h"

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

static char LOG_TAG[] = "SiftMatcher";

bool SiftMatcher::init(VulkanInstance *vulkan_instance)
{
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

bool SiftMatcher::initCommandPool()
{
  VkCommandPoolCreateInfo pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = 0, .queueFamilyIndex = m_queue_family_index};
  if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create command pool");
    return false;
  }

  return true;
}

bool SiftMatcher::initMemory()
{

  // Need IndirectDispatch buffers to run only on submitted number of SIFT
  {
    VkDeviceSize buffer_size = sizeof(uint32_t) * 3;
    if (!m_indispatch_buffer.create(m_device, m_physical_device, buffer_size,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
      logError(LOG_TAG, "Failed to create indirect dispatch buffer.");
      return false;
    }
  }
  // Indispatch staging in buffer
  {
    VkDeviceSize buffer_size = sizeof(uint32_t) * 3;
    if (!m_indispatch_staging_in_buffer.create(m_device, m_physical_device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
    {
      logError(LOG_TAG, "Failed to create indirect dispatch staging buffer.");
      return false;
    }
  }

  // Create staging buffers to upload SIFT data
  {
    if (!m_sift_a_staging_in_buffer.create(m_device, m_physical_device, sizeof(uint32_t) + (m_sift_buff_max_elem * sizeof(SIFT_Feature)),
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
    {
      logError(LOG_TAG, "Failed to create SIFT A staging buffer storage.");
      return false;
    }

    if (!m_sift_b_staging_in_buffer.create(m_device, m_physical_device, sizeof(uint32_t) + (m_sift_buff_max_elem * sizeof(SIFT_Feature)),
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
    {
      logError(LOG_TAG, "Failed to create SIFT B staging buffer storage.");
      return false;
    }
  }

  // Create buffers to store SIFT data
  {
    if (!m_sift_a_buffer.create(m_device, m_physical_device, sizeof(uint32_t) + (m_sift_buff_max_elem * sizeof(SIFT_Feature)),
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
      logError(LOG_TAG, "Failed to create SIFT A buffer storage.");
      return false;
    }

    if (!m_sift_b_buffer.create(m_device, m_physical_device, sizeof(uint32_t) + (m_sift_buff_max_elem * sizeof(SIFT_Feature)),
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
      logError(LOG_TAG, "Failed to create SIFT B buffer storage.");
      return false;
    }
  }

  // Create buffer to store 2NN results
  {
    if (!m_dists_buffer.create(m_device, m_physical_device, (m_sift_buff_max_elem * sizeof(SIFT_2NN_Info)),
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
      logError(LOG_TAG, "Failed to create 2NN result buffer storage.");
      return false;
    }
  }

  // Create staging buffer to send back SIFT data to CPU
  {
    if (!m_dists_staging_out_buffer.create(m_device, m_physical_device, (m_sift_buff_max_elem * sizeof(SIFT_2NN_Info)), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
    {
      logError(LOG_TAG, "Failed to create 2NN result staging buffer storage.");
      return false;
    }
  }

  if (vkMapMemory(m_device, m_sift_a_staging_in_buffer.getBufferMemory(), 0, VK_WHOLE_SIZE, 0, &m_input_sift_a_ptr) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to map input SIFT A memory.");
    return false;
  }
  if (vkMapMemory(m_device, m_sift_b_staging_in_buffer.getBufferMemory(), 0, VK_WHOLE_SIZE, 0, &m_input_sift_b_ptr) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to map input SIFT B memory.");
    return false;
  }

  if (vkMapMemory(m_device, m_dists_staging_out_buffer.getBufferMemory(), 0, VK_WHOLE_SIZE, 0, &m_output_dists_ptr) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to map output buffer memory.");
    return false;
  }

  if (vkMapMemory(m_device, m_indispatch_staging_in_buffer.getBufferMemory(), 0, VK_WHOLE_SIZE, 0, &m_indispatch_ptr) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to map indispatch buffer memory.");
    return false;
  }

  return true;
}
bool SiftMatcher::initDescriptors()
{
  ///////////////////////////////////////////////////
  // Descriptors for Get2NearestNeighbors pipeline
  ///////////////////////////////////////////////////
  {
    VkDescriptorSetLayoutBinding sift_a_buffer_layout_binding{.binding = 0,
                                                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              .descriptorCount = 1,
                                                              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                              .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding sift_b_buffer_layout_binding{.binding = 1,
                                                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              .descriptorCount = 1,
                                                              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                              .pImmutableSamplers = nullptr};
    VkDescriptorSetLayoutBinding dist_buffer_layout_binding{.binding = 2,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .pImmutableSamplers = nullptr};

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{sift_a_buffer_layout_binding, sift_b_buffer_layout_binding, dist_buffer_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = bindings.size(), .pBindings = bindings.data()};

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_nearestneighbor_desc_set_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create Get2NearestNeighbors descriptor set layout");
      return false;
    }

    // Create descriptor pool to allocate descriptor sets (generic)
    std::array<VkDescriptorPoolSize, 3> pool_sizes;
    pool_sizes[0] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1};
    pool_sizes[1] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1};
    pool_sizes[2] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1};
    VkDescriptorPoolCreateInfo descriptor_pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = pool_sizes.size(), .pPoolSizes = pool_sizes.data()};
    if (vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_nearestneighbor_desc_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create Get2NearestNeighbors descriptor pool");
      return false;
    }

    // Create descriptor sets that can be bound in command buffer
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = m_nearestneighbor_desc_pool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &m_nearestneighbor_desc_set_layout};

    if (vkAllocateDescriptorSets(m_device, &alloc_info, &m_nearestneighbor_desc_set) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate Get2NearestNeighbors descriptor set");
      return false;
    }

    // Write descriptor set
    VkDescriptorBufferInfo sift_a_buffer_info{.buffer = m_sift_a_buffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkDescriptorBufferInfo sift_b_buffer_info{.buffer = m_sift_b_buffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkDescriptorBufferInfo dist_buffer_info{.buffer = m_dists_buffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
    std::array<VkWriteDescriptorSet, 3> descriptor_writes;
    descriptor_writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = m_nearestneighbor_desc_set,
                            .dstBinding = 0,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .pImageInfo = nullptr,
                            .pBufferInfo = &sift_a_buffer_info,
                            .pTexelBufferView = nullptr};
    descriptor_writes[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = m_nearestneighbor_desc_set,
                            .dstBinding = 1,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .pImageInfo = nullptr,
                            .pBufferInfo = &sift_b_buffer_info,
                            .pTexelBufferView = nullptr};
    descriptor_writes[2] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = m_nearestneighbor_desc_set,
                            .dstBinding = 2,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            .pImageInfo = nullptr,
                            .pBufferInfo = &dist_buffer_info,
                            .pTexelBufferView = nullptr};
    vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
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

bool SiftMatcher::initPipelines()
{
  //////////////////////////////////////
  // Setup Get2NearestNeighbors pipeline
  //////////////////////////////////////
  {
    VkShaderModule nearestneighbor_shader_module;
    VulkanUtils::Shader::createShaderModule(m_device, "shaders/Get2NearestNeighbors.comp.spv", &nearestneighbor_shader_module);
    VkPipelineLayoutCreateInfo nearestneighbor_pipeline_layout_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                                    .setLayoutCount = 1,
                                                                    .pSetLayouts = &m_nearestneighbor_desc_set_layout,
                                                                    .pushConstantRangeCount = 0,
                                                                    .pPushConstantRanges = nullptr};
    if (vkCreatePipelineLayout(m_device, &nearestneighbor_pipeline_layout_info, nullptr, &m_nearestneighbor_pipeline_layout) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create Get2NearestNeighbors pipeline layout");
      return false;
    }

    VkPipelineShaderStageCreateInfo nearestneighbor_pipeline_shader_stage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                          .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                          .module = nearestneighbor_shader_module,
                                                                          .pName = "main"};

    VkComputePipelineCreateInfo nearestneighbor_pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                                 .pNext = nullptr,
                                                                 .flags = 0,
                                                                 .stage = nearestneighbor_pipeline_shader_stage,
                                                                 .layout = m_nearestneighbor_pipeline_layout,
                                                                 .basePipelineHandle = VK_NULL_HANDLE,
                                                                 .basePipelineIndex = -1};
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &nearestneighbor_pipeline_info, nullptr, &m_nearestneighbor_pipeline) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create Get2NearestNeighbors pipeline");
      return false;
    }
    vkDestroyShaderModule(m_device, nearestneighbor_shader_module, nullptr);
  }
  return true;
}

void SiftMatcher::beginMarkerRegion(VkCommandBuffer cmd_buf, const char *region_name)
{
  if (m_debug_marker_supported)
  {
    VkDebugMarkerMarkerInfoEXT marker_info{.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, .pMarkerName = region_name};
    m_vulkan_instance->vkCmdDebugMarkerBeginEXT(cmd_buf, &marker_info);
  }
}
void SiftMatcher::endMarkerRegion(VkCommandBuffer cmd_buf)
{
  if (m_debug_marker_supported)
  {
    m_vulkan_instance->vkCmdDebugMarkerEndEXT(cmd_buf);
  }
}

bool SiftMatcher::initCommandBuffer()
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
  vkCmdFillBuffer(m_command_buffer, m_sift_a_buffer.getBuffer(), 0, VK_WHOLE_SIZE, 0);
  vkCmdFillBuffer(m_command_buffer, m_sift_b_buffer.getBuffer(), 0, VK_WHOLE_SIZE, 0);
  vkCmdFillBuffer(m_command_buffer, m_dists_buffer.getBuffer(), 0, VK_WHOLE_SIZE, 0);
  endMarkerRegion(m_command_buffer);

  // Copy input data
  beginMarkerRegion(m_command_buffer, "CopyInputData");
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    buffer_barriers.push_back(m_indispatch_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT));
    buffer_barriers.push_back(m_sift_a_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT));
    buffer_barriers.push_back(m_sift_b_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT));
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  {
    VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = sizeof(uint32_t) * 3};
    vkCmdCopyBuffer(m_command_buffer, m_indispatch_staging_in_buffer.getBuffer(), m_indispatch_buffer.getBuffer(), 1, &region);
  }
  {
    VkBufferCopy region{.srcOffset = 0, .dstOffset = 0, .size = sizeof(uint32_t) + (sizeof(SIFT_Feature) * m_sift_buff_max_elem)};
    vkCmdCopyBuffer(m_command_buffer, m_sift_a_staging_in_buffer.getBuffer(), m_sift_a_buffer.getBuffer(), 1, &region);
    vkCmdCopyBuffer(m_command_buffer, m_sift_b_staging_in_buffer.getBuffer(), m_sift_b_buffer.getBuffer(), 1, &region);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    buffer_barriers.push_back(m_indispatch_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_INDIRECT_COMMAND_READ_BIT));
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    buffer_barriers.push_back(m_sift_a_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT));
    buffer_barriers.push_back(m_sift_b_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_SHADER_READ_BIT));
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  endMarkerRegion(m_command_buffer);

  beginMarkerRegion(m_command_buffer, "Get2NearestNeighbors");
  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_nearestneighbor_pipeline);

  vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_nearestneighbor_pipeline_layout, 0, 1, &m_nearestneighbor_desc_set, 0,
                          nullptr);
  vkCmdDispatchIndirect(m_command_buffer, m_indispatch_buffer.getBuffer(), 0);
  endMarkerRegion(m_command_buffer);

  beginMarkerRegion(m_command_buffer, "Copy2NN_Results");
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    buffer_barriers.push_back(m_dists_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_READ_BIT));
    buffer_barriers.push_back(m_dists_staging_out_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_TRANSFER_WRITE_BIT));
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  VkBufferCopy dist_copy_region{.srcOffset = 0u, .dstOffset = 0u, .size = sizeof(SIFT_2NN_Info) * m_sift_buff_max_elem};
  vkCmdCopyBuffer(m_command_buffer, m_dists_buffer.getBuffer(), m_dists_staging_out_buffer.getBuffer(), 1, &dist_copy_region);
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    buffer_barriers.push_back(m_dists_buffer.getBufferMemoryBarrierAndUpdate(0));
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, buffer_barriers.size(),
                         buffer_barriers.data(), 0, nullptr);
  }
  {
    std::vector<VkBufferMemoryBarrier> buffer_barriers;
    buffer_barriers.push_back(m_dists_staging_out_buffer.getBufferMemoryBarrierAndUpdate(VK_ACCESS_HOST_READ_BIT));
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

bool SiftMatcher::compute(const std::vector<SIFT_Feature> &sift_feats_a, const std::vector<SIFT_Feature> &sift_feats_b,
                          std::vector<SIFT_2NN_Info> &matches_info)
{
  vkResetFences(m_device, 1, &m_fence);
  // Copy SIFT A
  {
    VkMappedMemoryRange mem_range{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m_sift_a_staging_in_buffer.getBufferMemory(), .offset = 0u, .size = VK_WHOLE_SIZE};
    vkInvalidateMappedMemoryRanges(m_device, 1, &mem_range);
    uint32_t nb_sift = sift_feats_a.size();
    memcpy(m_input_sift_a_ptr, &nb_sift, sizeof(uint32_t));
    memcpy((uint32_t *)(m_input_sift_a_ptr) + 1, sift_feats_a.data(), sizeof(SIFT_Feature) * sift_feats_a.size());
    vkFlushMappedMemoryRanges(m_device, 1, &mem_range);
  }
  // Copy SIFT B
  {
    VkMappedMemoryRange mem_range{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m_sift_b_staging_in_buffer.getBufferMemory(), .offset = 0u, .size = VK_WHOLE_SIZE};
    vkInvalidateMappedMemoryRanges(m_device, 1, &mem_range);
    uint32_t nb_sift = sift_feats_b.size();
    memcpy(m_input_sift_b_ptr, &nb_sift, sizeof(uint32_t));
    memcpy((uint32_t *)(m_input_sift_b_ptr) + 1, sift_feats_b.data(), sizeof(SIFT_Feature) * sift_feats_b.size());
    vkFlushMappedMemoryRanges(m_device, 1, &mem_range);
  }
  // Fill in indispatch
  {
    VkMappedMemoryRange mem_range{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m_indispatch_buffer.getBufferMemory(), .offset = 0u, .size = VK_WHOLE_SIZE};
    vkInvalidateMappedMemoryRanges(m_device, 1, &mem_range);
    uint32_t nb_group_x = ceilf(static_cast<float>(sift_feats_a.size()) / 64);
    uint32_t indispatch_info[3] = {nb_group_x, 1, 1};
    memcpy(m_indispatch_ptr, &indispatch_info, sizeof(uint32_t) * 3);
    vkFlushMappedMemoryRanges(m_device, 1, &mem_range);
  }

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

  // Copy 2NN results back to CPU
  matches_info.resize(sift_feats_a.size());
  {
    VkMappedMemoryRange mem_range_output{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m_dists_staging_out_buffer.getBufferMemory(), .offset = 0u, .size = VK_WHOLE_SIZE};
    vkInvalidateMappedMemoryRanges(m_device, 1, &mem_range_output);
    memcpy(matches_info.data(), m_output_dists_ptr, sizeof(SIFT_2NN_Info) * sift_feats_a.size());
  }

  return true;
}

void SiftMatcher::terminate()
{
  vkQueueWaitIdle(m_queue);

  // Destroy sync objects and command buffers

  VK_NULL_SAFE_DELETE(m_fence, vkDestroyFence(m_device, m_fence, nullptr));
  vkFreeCommandBuffers(m_device, m_command_pool, 1, &m_command_buffer);

  // Destroy pipelines and descriptors
  // Get2NearestNeighbors
  VK_NULL_SAFE_DELETE(m_nearestneighbor_pipeline, vkDestroyPipeline(m_device, m_nearestneighbor_pipeline, nullptr));
  VK_NULL_SAFE_DELETE(m_nearestneighbor_pipeline_layout, vkDestroyPipelineLayout(m_device, m_nearestneighbor_pipeline_layout, nullptr));
  VK_NULL_SAFE_DELETE(m_nearestneighbor_desc_pool, vkDestroyDescriptorPool(m_device, m_nearestneighbor_desc_pool, nullptr));
  VK_NULL_SAFE_DELETE(m_nearestneighbor_desc_set_layout, vkDestroyDescriptorSetLayout(m_device, m_nearestneighbor_desc_set_layout, nullptr));

  // Destroy memory objects
  // Unmap before
  vkUnmapMemory(m_device, m_sift_a_staging_in_buffer.getBufferMemory());
  vkUnmapMemory(m_device, m_sift_b_staging_in_buffer.getBufferMemory());
  vkUnmapMemory(m_device, m_dists_staging_out_buffer.getBufferMemory());
  vkUnmapMemory(m_device, m_indispatch_staging_in_buffer.getBufferMemory());

  m_sift_a_staging_in_buffer.destroy(m_device);
  m_sift_a_buffer.destroy(m_device);
  m_sift_b_staging_in_buffer.destroy(m_device);
  m_sift_b_buffer.destroy(m_device);
  m_dists_staging_out_buffer.destroy(m_device);
  m_dists_buffer.destroy(m_device);
  m_indispatch_staging_in_buffer.destroy(m_device);
  m_indispatch_buffer.destroy(m_device);

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