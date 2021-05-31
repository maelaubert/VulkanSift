#include "sift_matcher.h"

#include "vkenv/logger.h"
#include "vkenv/vulkan_utils.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char LOG_TAG[] = "SiftMatcher";

static void getGPUDebugMarkerFuncs(vksift_SiftMatcher matcher)
{
  matcher->vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(matcher->dev->device, "vkCmdDebugMarkerBeginEXT");
  matcher->vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(matcher->dev->device, "vkCmdDebugMarkerEndEXT");
  matcher->debug_marker_supported = (matcher->vkCmdDebugMarkerBeginEXT != NULL) && (matcher->vkCmdDebugMarkerEndEXT != NULL);
}

static void beginMarkerRegion(vksift_SiftMatcher matcher, VkCommandBuffer cmd_buf, const char *region_name)
{
  if (matcher->debug_marker_supported)
  {
    VkDebugMarkerMarkerInfoEXT marker_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, .pMarkerName = region_name};
    matcher->vkCmdDebugMarkerBeginEXT(cmd_buf, &marker_info);
  }
}
static void endMarkerRegion(vksift_SiftMatcher matcher, VkCommandBuffer cmd_buf)
{
  if (matcher->debug_marker_supported)
  {
    matcher->vkCmdDebugMarkerEndEXT(cmd_buf);
  }
}

static bool setupCommandPools(vksift_SiftMatcher matcher)
{
  VkCommandPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                       .queueFamilyIndex = matcher->dev->general_queues_family_idx};
  if (vkCreateCommandPool(matcher->dev->device, &pool_info, NULL, &matcher->general_command_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create the general-purpose command pool");
    return false;
  }

  if (matcher->dev->async_transfer_available)
  {
    VkCommandPoolCreateInfo async_pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                               .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                               .queueFamilyIndex = matcher->dev->async_transfer_queues_family_idx};
    if (vkCreateCommandPool(matcher->dev->device, &async_pool_info, NULL, &matcher->async_transfer_command_pool) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create the asynchronous transfer command pool");
      return false;
    }
  }

  return true;
}

static bool allocateCommandBuffers(vksift_SiftMatcher matcher)
{
  VkCommandBufferAllocateInfo allocate_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                               .pNext = NULL,
                                               .commandPool = matcher->general_command_pool,
                                               .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                               .commandBufferCount = 1};
  if (vkAllocateCommandBuffers(matcher->dev->device, &allocate_info, &matcher->matching_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate the matching command buffe");
    return false;
  }

  // If the async tranfer queue is available the SIFT buffers are owned by the transfer queue family
  // in this case we need to release this ownership from the transfer queue before using the buffers in the general purpose queue
  if (matcher->dev->async_transfer_available)
  {
    allocate_info.commandPool = matcher->async_transfer_command_pool;
    if (vkAllocateCommandBuffers(matcher->dev->device, &allocate_info, &matcher->release_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate the release-buffer-ownership command buffer on the async transfer pool");
      return false;
    }
    if (vkAllocateCommandBuffers(matcher->dev->device, &allocate_info, &matcher->acquire_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to allocate the acquire-buffer-ownership command buffer on the async transfer pool");
      return false;
    }
  }

  return true;
}

static bool prepareDescriptorSets(vksift_SiftMatcher matcher)
{
  ///////////////////////////////////////////////////
  // Descriptors for Matching pipeline
  ///////////////////////////////////////////////////
  VkDescriptorSetLayoutBinding matching_sift_buffer_A_layout_binding = {.binding = 0,
                                                                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                        .descriptorCount = 1,
                                                                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                        .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding matching_sift_buffer_B_layout_binding = {.binding = 1,
                                                                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                        .descriptorCount = 1,
                                                                        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                        .pImmutableSamplers = NULL};
  VkDescriptorSetLayoutBinding matching_sift_matches_layout_binding = {.binding = 2,
                                                                       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                                       .descriptorCount = 1,
                                                                       .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                                       .pImmutableSamplers = NULL};

  VkDescriptorSetLayoutBinding matching_bindings[3] = {matching_sift_buffer_A_layout_binding, matching_sift_buffer_B_layout_binding,
                                                       matching_sift_matches_layout_binding};

  VkDescriptorSetLayoutCreateInfo matching_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 3, .pBindings = matching_bindings};

  if (vkCreateDescriptorSetLayout(matcher->dev->device, &matching_layout_info, NULL, &matcher->matching_desc_set_layout) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create Matching descriptor set layout");
    return false;
  }

  // Create descriptor pool to allocate descriptor sets (generic)
  VkDescriptorPoolSize matching_pool_sizes[3];
  matching_pool_sizes[0] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1};
  matching_pool_sizes[1] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1};
  matching_pool_sizes[2] = (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1};
  VkDescriptorPoolCreateInfo matching_descriptor_pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 3, .pPoolSizes = matching_pool_sizes};
  if (vkCreateDescriptorPool(matcher->dev->device, &matching_descriptor_pool_info, NULL, &matcher->matching_desc_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create Matching descriptor pool");
    return false;
  }

  // Create descriptor sets that can be bound in command buffer
  VkDescriptorSetAllocateInfo matching_alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                     .descriptorPool = matcher->matching_desc_pool,
                                                     .descriptorSetCount = 1,
                                                     .pSetLayouts = &matcher->matching_desc_set_layout};

  if (vkAllocateDescriptorSets(matcher->dev->device, &matching_alloc_info, &matcher->matching_desc_set) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate Matching descriptor set");
    return false;
  }

  return true;
}

static bool setupComputePipelines(vksift_SiftMatcher matcher)
{
  //////////////////////////////////////
  // Setup Matching pipeline
  //////////////////////////////////////
  VkShaderModule matching_shader_module;
  if (!vkenv_createShaderModule(matcher->dev->device, "shaders/Get2NearestNeighbors.comp.spv", &matching_shader_module))
  {
    logError(LOG_TAG, "Failed to create Matching shader module");
    return false;
  }
  if (!vkenv_createComputePipeline(matcher->dev->device, matching_shader_module, matcher->matching_desc_set_layout, 0, &matcher->matching_pipeline_layout,
                                   &matcher->matching_pipeline))
  {
    logError(LOG_TAG, "Failed to create Matching pipeline");
    vkDestroyShaderModule(matcher->dev->device, matching_shader_module, NULL);
    return false;
  }
  vkDestroyShaderModule(matcher->dev->device, matching_shader_module, NULL);
  return true;
}

static bool setupSyncObjects(vksift_SiftMatcher matcher)
{
  VkSemaphoreCreateInfo semaphore_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = NULL, .flags = 0};
  if (vkCreateSemaphore(matcher->dev->device, &semaphore_create_info, NULL, &matcher->end_of_matching_semaphore) != VK_SUCCESS ||
      vkCreateSemaphore(matcher->dev->device, &semaphore_create_info, NULL, &matcher->end_of_empty_buffer_semaphore) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create end-of-matching Vulkan semaphore");
    return false;
  }

  if (matcher->dev->async_transfer_available)
  {
    if (vkCreateSemaphore(matcher->dev->device, &semaphore_create_info, NULL, &matcher->buffer_ownership_released_by_transfer_semaphore) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create Vulkan semaphores for async transfer queue buffer ownership transfers");
      return false;
    }
  }

  VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
  if (vkCreateFence(matcher->dev->device, &fence_create_info, NULL, &matcher->end_of_matching_fence) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create a Vulkan fence");
    return false;
  }
  return true;
}

static bool writeDescriptorSets(vksift_SiftMatcher matcher)
{
  /////////////////////////////////////////////////////
  // Write sets for Matching pipeline
  VkDescriptorBufferInfo sift_buffer_A_info = {.buffer = matcher->mem->sift_buffer_arr[matcher->curr_buffer_A_idx], .offset = 0, .range = VK_WHOLE_SIZE};
  VkDescriptorBufferInfo sift_buffer_B_info = {.buffer = matcher->mem->sift_buffer_arr[matcher->curr_buffer_B_idx], .offset = 0, .range = VK_WHOLE_SIZE};
  VkDescriptorBufferInfo sift_matches_info = {.buffer = matcher->mem->match_output_buffer, .offset = 0, .range = VK_WHOLE_SIZE};
  VkWriteDescriptorSet descriptor_writes[3];
  descriptor_writes[0] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                .dstSet = matcher->matching_desc_set,
                                                .dstBinding = 0,
                                                .dstArrayElement = 0,
                                                .descriptorCount = 1,
                                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                .pImageInfo = NULL,
                                                .pBufferInfo = &sift_buffer_A_info,
                                                .pTexelBufferView = NULL};
  descriptor_writes[1] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                .dstSet = matcher->matching_desc_set,
                                                .dstBinding = 1,
                                                .dstArrayElement = 0,
                                                .descriptorCount = 1,
                                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                .pImageInfo = NULL,
                                                .pBufferInfo = &sift_buffer_B_info,
                                                .pTexelBufferView = NULL};
  descriptor_writes[2] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                .dstSet = matcher->matching_desc_set,
                                                .dstBinding = 2,
                                                .dstArrayElement = 0,
                                                .descriptorCount = 1,
                                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                .pImageInfo = NULL,
                                                .pBufferInfo = &sift_matches_info,
                                                .pTexelBufferView = NULL};
  vkUpdateDescriptorSets(matcher->dev->device, 3, descriptor_writes, 0, NULL);

  return true;
}

static void recMatchingCmds(vksift_SiftMatcher matcher, VkCommandBuffer cmdbuf)
{
  ///////////////////////////////
  // Matching
  ///////////////////////////////
  beginMarkerRegion(matcher, cmdbuf, "Matching");

  VkBufferMemoryBarrier buffer_barriers[3];
  // Set the access for the buffers
  buffer_barriers[0] = vkenv_genBufferMemoryBarrier(matcher->mem->sift_buffer_arr[matcher->curr_buffer_A_idx], 0, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0, VK_WHOLE_SIZE);
  buffer_barriers[1] = vkenv_genBufferMemoryBarrier(matcher->mem->sift_buffer_arr[matcher->curr_buffer_B_idx], 0, VK_ACCESS_SHADER_READ_BIT,
                                                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0, VK_WHOLE_SIZE);
  buffer_barriers[2] = vkenv_genBufferMemoryBarrier(matcher->mem->match_output_buffer, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED,
                                                    VK_QUEUE_FAMILY_IGNORED, 0, VK_WHOLE_SIZE);
  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 3, buffer_barriers, 0, NULL);

  // Run the matching pipeline
  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, matcher->matching_pipeline);
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, matcher->matching_pipeline_layout, 0, 1, &matcher->matching_desc_set, 0, NULL);
  vkCmdDispatch(cmdbuf, ceilf((float)(matcher->mem->curr_nb_matches) / 64.f), 1, 1);

  // Make sure the matches have been written before starting the transfer
  buffer_barriers[0] = vkenv_genBufferMemoryBarrier(matcher->mem->match_output_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0, VK_WHOLE_SIZE);

  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, buffer_barriers, 0, NULL);
  if (matcher->mem->curr_nb_matches > 0)
  {
    VkBufferCopy matches_copy_region = {.srcOffset = 0, .dstOffset = 0, .size = sizeof(vksift_Match_2NN) * matcher->mem->curr_nb_matches};
    vkCmdCopyBuffer(cmdbuf, matcher->mem->match_output_buffer, matcher->mem->match_output_staging_buffer, 1, &matches_copy_region);
  }
  endMarkerRegion(matcher, cmdbuf);
}

static void recBufferOwnershipTransferCmds(vksift_SiftMatcher matcher, VkCommandBuffer cmdbuf, const uint32_t src_queue_family_idx,
                                           const uint32_t dst_queue_family_idx, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage)
{
  beginMarkerRegion(matcher, cmdbuf, "BufferOwnershipTransfer");

  VkBufferMemoryBarrier ownership_barriers[3];
  ownership_barriers[0] = vkenv_genBufferMemoryBarrier(matcher->mem->sift_buffer_arr[matcher->curr_buffer_A_idx], 0, 0, src_queue_family_idx,
                                                       dst_queue_family_idx, 0, VK_WHOLE_SIZE);
  ownership_barriers[1] = vkenv_genBufferMemoryBarrier(matcher->mem->sift_buffer_arr[matcher->curr_buffer_B_idx], 0, 0, src_queue_family_idx,
                                                       dst_queue_family_idx, 0, VK_WHOLE_SIZE);
  ownership_barriers[2] =
      vkenv_genBufferMemoryBarrier(matcher->mem->match_output_buffer, 0, 0, src_queue_family_idx, dst_queue_family_idx, 0, VK_WHOLE_SIZE);
  vkCmdPipelineBarrier(cmdbuf, src_stage, dst_stage, 0, 0, NULL, 3, ownership_barriers, 0, NULL);

  endMarkerRegion(matcher, cmdbuf);
}

static bool recordCommandBuffers(vksift_SiftMatcher matcher)
{
  VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0, .pInheritanceInfo = NULL};

  /////////////////////////////////////////////////////
  // If async transfer queue is used, record ownership transfer command buffers
  /////////////////////////////////////////////////////
  if (matcher->dev->async_transfer_available)
  {
    if (vkBeginCommandBuffer(matcher->release_buffer_ownership_command_buffer, &begin_info) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to begin the release-buffer-ownership command buffer recording");
      return false;
    }
    recBufferOwnershipTransferCmds(matcher, matcher->release_buffer_ownership_command_buffer, matcher->dev->async_transfer_queues_family_idx,
                                   matcher->dev->general_queues_family_idx, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if (vkEndCommandBuffer(matcher->release_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to record release-buffer-ownership command buffer");
      return false;
    }

    if (vkBeginCommandBuffer(matcher->acquire_buffer_ownership_command_buffer, &begin_info) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to begin the acquire-buffer-ownership command buffer recording");
      return false;
    }
    recBufferOwnershipTransferCmds(matcher, matcher->acquire_buffer_ownership_command_buffer, matcher->dev->general_queues_family_idx,
                                   matcher->dev->async_transfer_queues_family_idx, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    if (vkEndCommandBuffer(matcher->acquire_buffer_ownership_command_buffer) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to record acquire-buffer-ownership command buffer");
      return false;
    }
  }

  /////////////////////////////////////////////////////
  // Write the matcher command buffer (single queue version)
  /////////////////////////////////////////////////////
  if (vkBeginCommandBuffer(matcher->matching_command_buffer, &begin_info) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to begin the command buffer recording");
    return false;
  }

  // We start using the SIFT buffer, is the async transfer is used we need to acquire the buffer ownership before using it
  if (matcher->dev->async_transfer_available)
  {
    recBufferOwnershipTransferCmds(matcher, matcher->matching_command_buffer, matcher->dev->async_transfer_queues_family_idx,
                                   matcher->dev->general_queues_family_idx, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }

  recMatchingCmds(matcher, matcher->matching_command_buffer);

  // No more operation with the buffer we can release the buffer ownership if needed
  if (matcher->dev->async_transfer_available)
  {
    recBufferOwnershipTransferCmds(matcher, matcher->matching_command_buffer, matcher->dev->general_queues_family_idx,
                                   matcher->dev->async_transfer_queues_family_idx, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  }

  if (vkEndCommandBuffer(matcher->matching_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to record command buffer");
    return false;
  }

  return true;
}

bool vksift_createSiftMatcher(vkenv_Device device, vksift_SiftMemory memory, vksift_SiftMatcher *matcher_ptr)
{
  assert(device != NULL);
  assert(memory != NULL);
  assert(matcher_ptr != NULL);
  assert(*matcher_ptr == NULL);
  *matcher_ptr = (vksift_SiftMatcher)malloc(sizeof(struct vksift_SiftMatcher_T));
  vksift_SiftMatcher matcher = *matcher_ptr;
  memset(matcher, 0, sizeof(struct vksift_SiftMatcher_T));

  // Store parent device and memory
  matcher->dev = device;
  matcher->mem = memory;

  // Assign queues
  matcher->general_queue = device->general_queues[0];
  if (device->async_transfer_available)
  {
    matcher->async_ownership_transfer_queue = device->async_transfer_queues[1]; // queue 0 used by SiftMemory only
  }

  matcher->curr_buffer_A_idx = 0u;
  matcher->curr_buffer_B_idx = (memory->nb_sift_buffer > 1) ? 1u : 0u;

  // Try to find GPU debug marker functions
  getGPUDebugMarkerFuncs(matcher);

  if (setupCommandPools(matcher) && allocateCommandBuffers(matcher) && prepareDescriptorSets(matcher) && setupComputePipelines(matcher) &&
      setupSyncObjects(matcher) && writeDescriptorSets(matcher) && recordCommandBuffers(matcher))
  {
    return true;
  }
  else
  {
    logError(LOG_TAG, "Failed to setup the SiftMatcher instance");
    return false;
  }
}

bool vksift_dispatchSiftMatching(vksift_SiftMatcher matcher, const uint32_t target_buffer_A_idx, const uint32_t target_buffer_B_idx)
{
  matcher->curr_buffer_A_idx = target_buffer_A_idx;
  matcher->curr_buffer_B_idx = target_buffer_B_idx;
  // We always rewrite the descriptors et command buffers (they depend on the number of features contained inside the buffer A)
  writeDescriptorSets(matcher);
  recordCommandBuffers(matcher);

  // Mark the buffers as busy/GPU locked
  vkResetFences(matcher->dev->device, 1, &matcher->end_of_matching_fence);

  VkPipelineStageFlags wait_dst_transfer_bit_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkPipelineStageFlags wait_dst_compute_shader_bit_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

  VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = NULL};

  if (matcher->dev->async_transfer_available)
  {
    // Transfer SIFT buffer ownership for the matching
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &matcher->release_buffer_ownership_command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &matcher->buffer_ownership_released_by_transfer_semaphore;
    if (vkQueueSubmit(matcher->async_ownership_transfer_queue, 1, &submit_info, NULL) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to submit ownership-release command buffer on async transfer queue");
      return false;
    }
  }

  // Matching pipeline
  if (matcher->dev->async_transfer_available)
  {
    // wait for buffer ownership transfer to complete
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &matcher->buffer_ownership_released_by_transfer_semaphore;
    submit_info.pWaitDstStageMask = &wait_dst_compute_shader_bit_stage_mask;
  }
  else
  {
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
  }
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &matcher->matching_command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &matcher->end_of_matching_semaphore;
  VkFence match_submit_fence = matcher->dev->async_transfer_available ? NULL : matcher->end_of_matching_fence;
  if (vkQueueSubmit(matcher->general_queue, 1, &submit_info, match_submit_fence) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to submit matching command buffer");
    return false;
  }

  if (matcher->dev->async_transfer_available)
  {
    // Give back SIFT buffer ownership to the main memory
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &matcher->end_of_matching_semaphore;
    submit_info.pWaitDstStageMask = &wait_dst_transfer_bit_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &matcher->acquire_buffer_ownership_command_buffer;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;
    if (vkQueueSubmit(matcher->async_ownership_transfer_queue, 1, &submit_info, matcher->end_of_matching_fence) != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to submit ownership-release command buffer on async transfer queue");
      return false;
    }
  }

  return true;
}

void vksift_destroySiftMatcher(vksift_SiftMatcher *matcher_ptr)
{
  assert(matcher_ptr != NULL);
  assert(*matcher_ptr != NULL); // vksift_destroySiftMatcher shouldn't be called on NULL vksift_SiftMemory object
  vksift_SiftMatcher matcher = *matcher_ptr;

  // Destroy sync objects
  VK_NULL_SAFE_DELETE(matcher->end_of_matching_semaphore, vkDestroySemaphore(matcher->dev->device, matcher->end_of_matching_semaphore, NULL));
  VK_NULL_SAFE_DELETE(matcher->end_of_empty_buffer_semaphore, vkDestroySemaphore(matcher->dev->device, matcher->end_of_empty_buffer_semaphore, NULL));
  VK_NULL_SAFE_DELETE(matcher->end_of_matching_fence, vkDestroyFence(matcher->dev->device, matcher->end_of_matching_fence, NULL));
  if (matcher->dev->async_transfer_available)
  {
    VK_NULL_SAFE_DELETE(matcher->buffer_ownership_released_by_transfer_semaphore,
                        vkDestroySemaphore(matcher->dev->device, matcher->buffer_ownership_released_by_transfer_semaphore, NULL));
  }

  // Destroy command pools
  VK_NULL_SAFE_DELETE(matcher->general_command_pool, vkDestroyCommandPool(matcher->dev->device, matcher->general_command_pool, NULL));
  if (matcher->dev->async_transfer_available)
  {
    VK_NULL_SAFE_DELETE(matcher->async_transfer_command_pool, vkDestroyCommandPool(matcher->dev->device, matcher->async_transfer_command_pool, NULL));
  }

  // Destroy pipelines and descriptors
  // Matching
  VK_NULL_SAFE_DELETE(matcher->matching_pipeline, vkDestroyPipeline(matcher->dev->device, matcher->matching_pipeline, NULL));
  VK_NULL_SAFE_DELETE(matcher->matching_pipeline_layout, vkDestroyPipelineLayout(matcher->dev->device, matcher->matching_pipeline_layout, NULL));
  VK_NULL_SAFE_DELETE(matcher->matching_desc_pool, vkDestroyDescriptorPool(matcher->dev->device, matcher->matching_desc_pool, NULL));
  VK_NULL_SAFE_DELETE(matcher->matching_desc_set_layout, vkDestroyDescriptorSetLayout(matcher->dev->device, matcher->matching_desc_set_layout, NULL));

  // Release vksift_SiftMatcher memory
  free(*matcher_ptr);
  *matcher_ptr = NULL;
}