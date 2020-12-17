#include "vulkansift/utils/vulkan_utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "vulkansift/utils/logger.h"

namespace VulkanUtils
{

static char LOG_TAG[] = "VulkanUtils";

////////////////////////////////////////////////////////////////////////
// SHADER
////////////////////////////////////////////////////////////////////////
namespace Shader
{
#ifdef VK_USE_PLATFORM_ANDROID_KHR
bool is_asset_access_ok = false;
AAssetManager *asset_manager = nullptr;
void setupAndroidAssetsFileAccess(AAssetManager *android_asset_manager)
{
  asset_manager = android_asset_manager;
  is_asset_access_ok = true;
}

bool createShaderModuleFromAsset(VkDevice device, const char *shader_asset_file_path, VkShaderModule *shader_module)
{
  if (!is_asset_access_ok)
  {
    logError(LOG_TAG, "On Android, setupAndroidAssetsFileAccess must be called once before createShaderModuleFromAsset");
    return false;
  }

  // Read shader code from file
  AAsset *f_shader_file = AAssetManager_open(asset_manager, shader_asset_file_path, AASSET_MODE_BUFFER);
  if (f_shader_file == NULL)
  {
    logError(LOG_TAG, "Failed to open shader asset file %s", shader_asset_file_path);
    return false;
  }
  size_t shader_code_size = AAsset_getLength(f_shader_file);
  char *shader_code = (char *)malloc((shader_code_size + 1) * sizeof(char));
  memset(shader_code, 0, shader_code_size + 1);
  AAsset_read(f_shader_file, shader_code, shader_code_size);
  AAsset_close(f_shader_file);

  VkShaderModuleCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                       .codeSize = (size_t)shader_code_size,
                                       .pCode = reinterpret_cast<const uint32_t *>(shader_code)};
  if (vkCreateShaderModule(device, &create_info, nullptr, shader_module) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create shader module from asset file %s", shader_asset_file_path);
    free(shader_code);
    return false;
  }

  free(shader_code);
  return true;
}
#endif

bool createShaderModuleFromFile(VkDevice device, const char *shader_file_path, VkShaderModule *shader_module)
{
  // Read shader code from file
  FILE *f_shader_file = fopen(shader_file_path, "r");
  if (f_shader_file == NULL)
  {
    logError(LOG_TAG, "Failed to open shader file %s", shader_file_path);
    return false;
  }
  fseek(f_shader_file, 0L, SEEK_END);
  long shader_code_size = ftell(f_shader_file);
  rewind(f_shader_file);
  char *shader_code = (char *)malloc((shader_code_size + 1) * sizeof(char));
  memset(shader_code, 0, shader_code_size + 1);
  int read_res = fread(shader_code, shader_code_size, 1, f_shader_file);
  fclose(f_shader_file);

  VkShaderModuleCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                       .codeSize = (size_t)shader_code_size,
                                       .pCode = reinterpret_cast<const uint32_t *>(shader_code)};
  if (vkCreateShaderModule(device, &create_info, nullptr, shader_module) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create shader module from file %s", shader_file_path);
    free(shader_code);
    return false;
  }

  free(shader_code);
  return true;
}

bool createShaderModule(VkDevice device, const char *shader_file_path, VkShaderModule *shader_module)
{
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  return createShaderModuleFromAsset(device, shader_file_path, shader_module);
#else
  return createShaderModuleFromFile(device, shader_file_path, shader_module);
#endif
}
} // namespace Shader
////////////////////////////////////////////////////////////////////////
// IMAGE
////////////////////////////////////////////////////////////////////////
bool Image::create(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                   VkImageUsageFlags usage, VkMemoryPropertyFlags memory_properties, uint32_t array_nb_layer)
{
  if (array_nb_layer > 0)
  {
    m_is_array = true;
    m_nb_layer = array_nb_layer;
  }
  else
  {
    m_nb_layer = 1;
  }

  // Create vkImage instance
  VkImageCreateInfo image_info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                               .imageType = VK_IMAGE_TYPE_2D,
                               .format = format,
                               .extent = {width, height, 1},
                               .mipLevels = 1,
                               .arrayLayers = m_nb_layer,
                               .samples = VK_SAMPLE_COUNT_1_BIT,
                               .tiling = tiling,
                               .usage = usage,
                               .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                               .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

  if (vkCreateImage(device, &image_info, nullptr, &m_image) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create image");
    return false;
  }

  // Find and allocate memory for the image
  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(device, m_image, &memory_requirements);
  VkPhysicalDeviceMemoryProperties available_memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &available_memory_properties);
  uint32_t memory_type_idx = 0;
  bool mem_type_found = false;

  for (uint32_t i = 0; i < available_memory_properties.memoryTypeCount; i++)
  {
    if ((memory_requirements.memoryTypeBits & (1 << i)) &&
        (available_memory_properties.memoryTypes[i].propertyFlags & memory_properties) == memory_properties)
    {
      memory_type_idx = i;
      mem_type_found = true;
      break;
    }
  }
  if (!mem_type_found)
  {
    logError(LOG_TAG, "Failed to find a valid memory type for the image");
    return false;
  }

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memory_requirements.size;
  allocInfo.memoryTypeIndex = memory_type_idx;

  if (vkAllocateMemory(device, &allocInfo, nullptr, &m_image_memory) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate image memory");
    return false;
  }

  if (vkBindImageMemory(device, m_image, m_image_memory, 0) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to bind memory to image");
    return false;
  }

  // Create the corresponding image view
  VkImageViewCreateInfo image_view_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = m_image,
      .viewType = m_is_array ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = m_nb_layer}};
  if (vkCreateImageView(device, &image_view_create_info, nullptr, &m_image_view) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create the image view");
    return false;
  }

  // Init default values for image layout, access and stage masks
  m_image_access_mask = 0;
  m_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_image_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

  return true;
}

VkImageMemoryBarrier Image::getImageMemoryBarrierAndUpdate(VkAccessFlags dst_access_mask, VkImageLayout dst_layout)
{
  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = m_image_access_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = m_image_layout,
      .newLayout = dst_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = m_image,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = m_nb_layer}};
  m_image_access_mask = dst_access_mask;
  m_image_layout = dst_layout;
  return barrier;
}

void Image::destroy(VkDevice device)
{
  // Destroy image view
  if (m_image_view != VK_NULL_HANDLE)
  {
    vkDestroyImageView(device, m_image_view, nullptr);
    m_image_view = VK_NULL_HANDLE;
  }

  // Destroy image
  if (m_image != VK_NULL_HANDLE)
  {
    vkDestroyImage(device, m_image, nullptr);
    m_image = VK_NULL_HANDLE;
  }

  // Free memory
  if (m_image_memory != VK_NULL_HANDLE)
  {
    vkFreeMemory(device, m_image_memory, nullptr);
    m_image_memory = VK_NULL_HANDLE;
  }
}
////////////////////////////////////////////////////////////////////////
// BUFFER
////////////////////////////////////////////////////////////////////////
bool Buffer::create(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags memory_properties)
{
  // Create buffer
  VkBufferCreateInfo buff_create_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
  if (vkCreateBuffer(device, &buff_create_info, nullptr, &m_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create buffer");
    return false;
  }

  // Find and allocate memory for the image
  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device, m_buffer, &memory_requirements);
  VkPhysicalDeviceMemoryProperties available_memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &available_memory_properties);
  uint32_t memory_type_idx = 0;
  bool mem_type_found = false;

  for (uint32_t i = 0; i < available_memory_properties.memoryTypeCount; i++)
  {
    if ((memory_requirements.memoryTypeBits & (1 << i)) &&
        (available_memory_properties.memoryTypes[i].propertyFlags & memory_properties) == memory_properties)
    {
      memory_type_idx = i;
      mem_type_found = true;
      break;
    }
  }
  if (!mem_type_found)
  {
    logError(LOG_TAG, "Failed to find a valid memory type for the buffer");
    return false;
  }
  VkMemoryAllocateInfo mem_alloc_info{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = memory_requirements.size, .memoryTypeIndex = memory_type_idx};
  if (vkAllocateMemory(device, &mem_alloc_info, nullptr, &m_buffer_memory) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate buffer memory");
    return false;
  }

  if (vkBindBufferMemory(device, m_buffer, m_buffer_memory, 0) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to bind memory to buffer");
    return false;
  }

  m_buffer_size = size;
  m_buffer_access_mask = 0;
  m_buffer_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

  return true;
}

VkBufferMemoryBarrier Buffer::getBufferMemoryBarrierAndUpdate(VkAccessFlags dst_access_mask)
{
  VkBufferMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                .srcAccessMask = m_buffer_access_mask,
                                .dstAccessMask = dst_access_mask,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .buffer = m_buffer,
                                .offset = 0,
                                .size = m_buffer_size};
  m_buffer_access_mask = dst_access_mask;
  return barrier;
}

void Buffer::destroy(VkDevice device)
{
  // Destroy buffer and memory
  if (m_buffer != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(device, m_buffer, nullptr);
    m_buffer = VK_NULL_HANDLE;
  }
  // Free memory
  if (m_buffer_memory != VK_NULL_HANDLE)
  {
    vkFreeMemory(device, m_buffer_memory, nullptr);
    m_buffer_memory = VK_NULL_HANDLE;
  }
}
////////////////////////////////////////////////////////////////////////
// MISC
////////////////////////////////////////////////////////////////////////
bool submitCommandsAndWait(VkDevice device, VkQueue queue, VkCommandPool pool, std::function<void(VkCommandBuffer cmd_buff)> commands_func)
{
  VkCommandBuffer command_buffer;
  VkCommandBufferAllocateInfo command_buff_alloc_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};

  if (vkAllocateCommandBuffers(device, &command_buff_alloc_info, &command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate command buffer in submitCommandsAndWait");
    return false;
  }

  // Begin command recording
  VkCommandBufferBeginInfo command_buff_begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  if (vkBeginCommandBuffer(command_buffer, &command_buff_begin_info) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to begin command buffer in submitCommandsAndWait");
    return false;
  }

  // Register commands into command buffer
  commands_func(command_buffer);

  // End recording
  if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to end command buffer in submitCommandsAndWait");
    return false;
  }

  // Submit command buffer to queue
  VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &command_buffer};
  if (vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to submit command buffer in submitCommandsAndWait");
    return false;
  }

  // Wait for execution to finish
  if (vkQueueWaitIdle(queue) != VK_SUCCESS)
  {
    logError(LOG_TAG, "vkQueueWaitIdle failed in submitCommandsAndWait");
    return false;
  }

  // Free command buffer
  vkFreeCommandBuffers(device, pool, 1, &command_buffer);
  return true;
}
////////////////////////////////////////////////////////////////////////

} // namespace VulkanUtils