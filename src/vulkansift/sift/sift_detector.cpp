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
  // TODO
  return true;
}
bool SiftDetector::initDescriptors()
{
  // TODO
  return true;
}
bool SiftDetector::initPipelines()
{
  // TODO
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

  if (vkEndCommandBuffer(m_command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to record command buffer");
    return false;
  }
  return true;
}

bool SiftDetector::compute(uint8_t *pixel_buffer, std::vector<SIFT_Feature> &sift_feats) { return true; }

void SiftDetector::terminate()
{
  vkQueueWaitIdle(m_queue);

  // Destroy sync objects and command buffers

  VK_NULL_SAFE_DELETE(m_fence, vkDestroyFence(m_device, m_fence, nullptr));
  vkFreeCommandBuffers(m_device, m_command_pool, 1, &m_command_buffer);

  // Destroy pipelines and descriptors
  // TODO

  // Destroy memory objects
  // Unmap before
  // TODO

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