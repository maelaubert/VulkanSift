#ifndef SIFT_MATCHER_H
#define SIFT_MATCHER_H

#include "vulkansift/sift/sift_feature.h"
#include "vulkansift/utils/vulkan_utils.h"
#include "vulkansift/vulkan_instance.h"

#include <cmath>

namespace VulkanSIFT
{

struct SIFT_2NN_Info
{
  uint32_t idx_a;
  uint32_t idx_b1;
  uint32_t idx_b2;
  float dist_ab1;
  float dist_ab2;
};

class SiftMatcher
{
  public:
  SiftMatcher() = default;
  bool init(VulkanInstance *vulkan_instance);
  bool compute(const std::vector<SIFT_Feature> &sift_feats_a, const std::vector<SIFT_Feature> &sift_feats_b, std::vector<SIFT_Feature> &matches_a,
               std::vector<SIFT_Feature> &matches_b);
  void terminate();

  private:
  bool initCommandPool();
  bool initMemory();
  bool initDescriptors();
  bool initPipelines();
  bool initCommandBuffer();

  void beginMarkerRegion(VkCommandBuffer cmd_buf, const char *region_name);
  void endMarkerRegion(VkCommandBuffer cmd_buf);

  // VulkanManager related
  VulkanInstance *m_vulkan_instance = nullptr;
  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkQueue m_queue = VK_NULL_HANDLE;
  uint32_t m_queue_family_index = 0u;
  VkQueue m_async_queue = VK_NULL_HANDLE;
  uint32_t m_async_queue_family_index = 0u;
  bool m_debug_marker_supported = false;

  ///////////////////////////////////////////////////////////////////////////////

  ////////////////////
  // Memory objects
  //
  // Buffers
  VulkanUtils::Buffer m_sift_a_staging_in_buffer;
  VulkanUtils::Buffer m_sift_b_staging_in_buffer;
  VulkanUtils::Buffer m_dists_staging_out_buffer;
  uint32_t m_sift_buff_max_elem = 50000;
  VulkanUtils::Buffer m_sift_a_buffer;
  VulkanUtils::Buffer m_sift_b_buffer;
  VulkanUtils::Buffer m_dists_buffer;
  VulkanUtils::Buffer m_indispatch_staging_in_buffer;
  VulkanUtils::Buffer m_indispatch_buffer;

  ////////////////////

  ////////////////////
  // Commands related
  ////////////////////
  VkCommandBuffer m_command_buffer;
  VkCommandPool m_command_pool;

  ////////////////////
  // Pipelines and descriptors
  ////////////////////
  // Get2NearestNeighbors
  VkDescriptorSetLayout m_nearestneighbor_desc_set_layout = VK_NULL_HANDLE;
  VkDescriptorPool m_nearestneighbor_desc_pool = VK_NULL_HANDLE;
  VkDescriptorSet m_nearestneighbor_desc_set;
  VkPipelineLayout m_nearestneighbor_pipeline_layout = VK_NULL_HANDLE;
  VkPipeline m_nearestneighbor_pipeline = VK_NULL_HANDLE;

  void *m_input_sift_a_ptr = nullptr;
  void *m_input_sift_b_ptr = nullptr;
  void *m_output_dists_ptr = nullptr;
  void *m_indispatch_ptr = nullptr;

  VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace VulkanSIFT

#endif // SIFT_MATCHER_H