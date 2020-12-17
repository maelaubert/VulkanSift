#ifndef VULKAN_UTILS_H
#define VULKAN_UTILS_H

#include "vulkansift/utils/vulkan_loader.h"
#include <functional>

#ifdef VK_USE_PLATFORM_ANDROID_KHR
#include <android/asset_manager.h>
#endif

#define VK_NULL_SAFE_DELETE(var_name, var_deleter)                                                                                                        \
  {                                                                                                                                                       \
    if (var_name != VK_NULL_HANDLE)                                                                                                                       \
    {                                                                                                                                                     \
      var_deleter;                                                                                                                                        \
      var_name = VK_NULL_HANDLE;                                                                                                                          \
    }                                                                                                                                                     \
  }

namespace VulkanUtils
{

////////////////////////////////////////////////////////////////////////
// SHADER
////////////////////////////////////////////////////////////////////////
namespace Shader
{
#ifdef VK_USE_PLATFORM_ANDROID_KHR
void setupAndroidAssetsFileAccess(AAssetManager *android_asset_manager);
#endif
bool createShaderModule(VkDevice device, const char *shader_file_path, VkShaderModule *shader_module);
} // namespace Shader

////////////////////////////////////////////////////////////////////////
// IMAGE
////////////////////////////////////////////////////////////////////////
class Image
{
  public:
  bool create(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
              VkImageUsageFlags usage, VkMemoryPropertyFlags memory_properties, uint32_t nb_layer = 0u);
  VkImageMemoryBarrier getImageMemoryBarrierAndUpdate(VkAccessFlags dst_access_mask, VkImageLayout dst_layout);
  void destroy(VkDevice device);

  VkImage getImage() { return m_image; }
  VkImageView getImageView() { return m_image_view; }

  private:
  VkImage m_image = VK_NULL_HANDLE;
  VkDeviceMemory m_image_memory = VK_NULL_HANDLE;
  VkImageView m_image_view = VK_NULL_HANDLE;

  VkAccessFlags m_image_access_mask = 0;
  VkImageLayout m_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkPipelineStageFlags m_image_stage_mask = 0;
  bool m_is_array = false;
  uint32_t m_nb_layer = 1;
};
////////////////////////////////////////////////////////////////////////
// BUFFER
////////////////////////////////////////////////////////////////////////
class Buffer
{
  public:
  bool create(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties);
  VkBufferMemoryBarrier getBufferMemoryBarrierAndUpdate(VkAccessFlags dst_access_mask);
  void destroy(VkDevice device);

  VkBuffer getBuffer() { return m_buffer; }
  VkDeviceMemory getBufferMemory() { return m_buffer_memory; }
  VkDeviceSize getBufferSize() { return m_buffer_size; }

  private:
  VkBuffer m_buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_buffer_memory = VK_NULL_HANDLE;
  VkDeviceSize m_buffer_size = 0u;

  VkAccessFlags m_buffer_access_mask = 0;
  VkPipelineStageFlags m_buffer_stage_mask = 0;
};

////////////////////////////////////////////////////////////////////////
// MISC
////////////////////////////////////////////////////////////////////////
bool submitCommandsAndWait(VkDevice device, VkQueue queue, VkCommandPool pool, std::function<void(VkCommandBuffer cmd_buff)> commands_func);

}; // namespace VulkanUtils

#endif // VULKAN_UTILS_H