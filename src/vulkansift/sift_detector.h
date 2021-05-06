#ifndef VKSIFT_SIFTDETECTOR
#define VKSIFT_SIFTDETECTOR

#include "sift_memory.h"
#include "types.h"

#include "vkenv/vulkan_device.h"

#define VKSIFT_DETECTOR_MAX_GAUSSIAN_KERNEL_SIZE 20u

typedef struct vksift_SiftDetector_T
{
  vkenv_Device dev;      // parent device
  vksift_SiftMemory mem; // associated memory

  // Current buffer target for the SIFT detector
  // This defines where found features will be stored
  uint32_t curr_buffer_idx;

  VkCommandPool general_command_pool;
  VkCommandPool async_compute_command_pool;
  VkCommandBuffer sync_command_buffer;

  VkSampler image_sampler;

  // Sync objects
  VkFence end_of_detection_fence;

  bool debug_marker_supported;
  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;

  // Gaussian kernels
  uint32_t *gaussian_kernel_sizes;
  float *gaussian_kernels;

  // Gaussian Blur set
  VkDescriptorSetLayout blur_desc_set_layout;
  VkDescriptorPool blur_desc_pool;
  VkDescriptorSet *blur_desc_sets;
  VkDescriptorSet *blur_h_desc_sets;
  VkDescriptorSet *blur_v_desc_sets;
  VkPipelineLayout blur_pipeline_layout;
  VkPipeline blur_pipeline;
  // Difference of Gaussian set
  VkDescriptorSetLayout dog_desc_set_layout;
  VkDescriptorPool dog_desc_pool;
  VkDescriptorSet *dog_desc_sets;
  VkPipelineLayout dog_pipeline_layout;
  VkPipeline dog_pipeline;
  // ExtrackKeypoints set
  VkDescriptorSetLayout extractkpts_desc_set_layout;
  VkDescriptorPool extractkpts_desc_pool;
  VkDescriptorSet *extractkpts_desc_sets;
  VkPipelineLayout extractkpts_pipeline_layout;
  VkPipeline extractkpts_pipeline;
  // ComputeOrientation set
  VkDescriptorSetLayout orientation_desc_set_layout;
  VkDescriptorPool orientation_desc_pool;
  VkDescriptorSet *orientation_desc_sets;
  VkPipelineLayout orientation_pipeline_layout;
  VkPipeline orientation_pipeline;
  // ComputeDescriptor set
  VkDescriptorSetLayout descriptor_desc_set_layout;
  VkDescriptorPool descriptor_desc_pool;
  VkDescriptorSet *descriptor_desc_sets;
  VkPipelineLayout descriptor_pipeline_layout;
  VkPipeline descriptor_pipeline;

  // Config
  bool use_hardware_interp_kernel;
  float input_blur_level;
  float seed_scale_sigma;
  float intensity_threshold;
  float edge_threshold;

} * vksift_SiftDetector;

bool vksift_createSiftDetector(vkenv_Device device, vksift_SiftMemory memory, vksift_SiftDetector *detector_ptr, const vksift_Config *config);
void vksift_destroySiftDetector(vksift_SiftDetector *detector_ptr);

bool vksift_dispatchSiftDetector(vksift_SiftDetector detector, const uint32_t target_buffer_idx, const bool memory_layout_updated);

#endif // VKSIFT_SIFTDETECTOR