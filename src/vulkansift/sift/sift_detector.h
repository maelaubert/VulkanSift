#ifndef SIFT_DETECTOR_H
#define SIFT_DETECTOR_H

#include "vulkansift/sift/sift_feature.h"
#include "vulkansift/utils/vulkan_utils.h"
#include "vulkansift/vulkan_instance.h"

namespace VulkanSIFT
{

class SiftDetector
{
  public:
  SiftDetector() = default;
  bool init(VulkanInstance *vulkan_instance, const int image_width, const int image_height);
  bool compute(uint8_t *pixel_buffer, std::vector<SIFT_Feature> &sift_feats);
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

  uint32_t m_image_width;
  uint32_t m_image_height;
  ////////////////////
  // Memory objects
  //
  // Images
  // TODO

  // Buffers
  // TODO

  ////////////////////

  ////////////////////
  // Commands related
  ////////////////////
  VkCommandBuffer m_command_buffer;
  VkCommandPool m_command_pool;

  ////////////////////
  // Pipelines and descriptors
  ////////////////////
  // TODO

  VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace VulkanSIFT

#endif // SIFT_DETECTOR_H