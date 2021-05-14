#ifndef VKSIFT_SIFTMATCHER
#define VKSIFT_SIFTMATCHER

#include "sift_memory.h"
#include "types.h"

#include "vkenv/vulkan_device.h"

typedef struct vksift_SiftMatcher_T
{
  vkenv_Device dev;      // parent device
  vksift_SiftMemory mem; // associated memory

  // Current SIFT buffers used as input for the matching
  uint32_t curr_buffer_A_idx;
  uint32_t curr_buffer_B_idx;

  VkQueue general_queue;
  VkQueue async_ownership_transfer_queue;

  VkCommandPool general_command_pool;
  VkCommandPool async_transfer_command_pool;

  VkCommandBuffer matching_command_buffer;
  VkCommandBuffer end_of_buffer_A_command_buffer;
  VkCommandBuffer end_of_buffer_B_command_buffer;
  VkCommandBuffer acquire_buffer_ownership_command_buffer;
  VkCommandBuffer release_buffer_ownership_command_buffer;

  // Sync objects
  VkFence end_of_matching_fence;
  VkSemaphore end_of_matching_semaphore;
  VkSemaphore end_of_empty_buffer_semaphore;
  VkSemaphore buffer_ownership_released_by_transfer_semaphore;
  VkSemaphore buffer_ownership_acquired_by_transfer_semaphore;

  bool debug_marker_supported;
  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;

  // Matching pipeline set
  VkDescriptorSetLayout matching_desc_set_layout;
  VkDescriptorPool matching_desc_pool;
  VkDescriptorSet matching_desc_set;
  VkPipelineLayout matching_pipeline_layout;
  VkPipeline matching_pipeline;
} * vksift_SiftMatcher;

bool vksift_createSiftMatcher(vkenv_Device device, vksift_SiftMemory memory, vksift_SiftMatcher *matcher_ptr, const vksift_Config *config);
void vksift_destroySiftMatcher(vksift_SiftMatcher *matcher_ptr);

bool vksift_dispatchSiftMatching(vksift_SiftMatcher matcher, const uint32_t target_buffer_A_idx, const uint32_t target_buffer_B_idx);

#endif // VKSIFT_SIFTMATCHER