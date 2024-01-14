#ifndef VKENV_UTILS_H
#define VKENV_UTILS_H

#include "vulkan_device.h"

#define VK_NULL_SAFE_DELETE(var_name, var_deleter)                                                                                                        \
  {                                                                                                                                                       \
    if (var_name != VK_NULL_HANDLE)                                                                                                                       \
    {                                                                                                                                                     \
      var_deleter;                                                                                                                                        \
      var_name = VK_NULL_HANDLE;                                                                                                                          \
    }                                                                                                                                                     \
  }

#define VKENV_DEFAULT_COMPONENT_MAPPING                                                                                                                   \
  (VkComponentMapping) { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY }

////////////////////////////////////////////////////////////////////////////////////////
// Instantaneous command buffer
// Allocate and start recording a new command buffer recording with beginInstantCommandBuffer()
// Stop recording, submit, wait for results and free the command buffer with endInstantCommandBuffer()
////////////////////////////////////////////////////////////////////////////////////////

bool vkenv_beginInstantCommandBuffer(VkDevice device, VkCommandPool pool, VkCommandBuffer *command_buffer);
bool vkenv_endInstantCommandBuffer(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer command_buffer);

////////////////////////////////////////////////////////////////////////
// SHADER
////////////////////////////////////////////////////////////////////////
#ifdef VKENV_USE_EMBEDDED_SHADERS
bool __vkenv_get_embedded_shader_code(const char *shader_path, uint32_t *shader_size, const uint8_t **shader_code);
#endif
bool vkenv_createShaderModule(VkDevice device, const char *shader_file_path, VkShaderModule *shader_module);

////////////////////////////////////////////////////////////////////////
// PIPELINE
////////////////////////////////////////////////////////////////////////

// Create a layout and a compute pipeline (this function doesn't handle shader specialization and pipeline derivatives)
bool vkenv_createComputePipeline(VkDevice device, VkShaderModule shader_module, VkDescriptorSetLayout descriptor_set_layout, uint32_t push_constant_size,
                                 VkPipelineLayout *pipeline_layout, VkPipeline *pipeline);

////////////////////////////////////////////////////////////////////////
// RESOURCES
////////////////////////////////////////////////////////////////////////

bool vkenv_allocateMemory(VkDeviceMemory *memory_ptr, vkenv_Device device, VkDeviceSize size, uint32_t memory_type_idx);
bool vkenv_findValidMemoryType(VkPhysicalDevice physical_device, VkMemoryRequirements memory_requirements, VkMemoryPropertyFlags memory_properties,
                               uint32_t *memory_type_idx);

////////////////////////////////////////////////////////////////////////
// IMAGES
////////////////////////////////////////////////////////////////////////

bool vkenv_createImage(VkImage *image_ptr, vkenv_Device device, VkImageCreateFlags flags, VkImageType image_type, VkFormat format, VkExtent3D extent,
                       uint32_t mip_levels, uint32_t array_layers, VkSampleCountFlags samples, VkImageTiling tiling, VkImageUsageFlags usage,
                       VkSharingMode sharing_mode, uint32_t queue_family_index_count, const uint32_t *queue_family_indices_ptr,
                       VkImageLayout initial_layout);
bool vkenv_createImageView(VkImageView *image_view_ptr, vkenv_Device device, VkImageViewCreateFlags flags, VkImage image, VkImageViewType view_type,
                           VkFormat format, VkComponentMapping components, VkImageSubresourceRange range);
bool vkenv_bindImageMemory(vkenv_Device device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset);
VkImageMemoryBarrier vkenv_genImageMemoryBarrier(VkImage image, VkAccessFlags src_mask, VkAccessFlags dst_mask, VkImageLayout old_layout,
                                                 VkImageLayout new_layout, uint32_t src_queue_family_idx, uint32_t dst_queue_family_idx,
                                                 VkImageSubresourceRange range);

////////////////////////////////////////////////////////////////////////
// BUFFER
////////////////////////////////////////////////////////////////////////

bool vkenv_createBuffer(VkBuffer *buffer_ptr, vkenv_Device device, VkBufferCreateFlags flags, VkDeviceSize size, VkBufferUsageFlags usage,
                        VkSharingMode sharing_mode, uint32_t queue_family_index_count, const uint32_t *queue_family_indices_ptr);
bool vkenv_bindBufferMemory(vkenv_Device device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset);
VkBufferMemoryBarrier vkenv_genBufferMemoryBarrier(VkBuffer buffer, VkAccessFlags src_mask, VkAccessFlags dst_mask, uint32_t src_queue_family_idx,
                                                   uint32_t dst_queue_family_idx, VkDeviceSize offset, VkDeviceSize size);

////////////////////////////////////////////////////////////////////////
// DEBUG UTILS
////////////////////////////////////////////////////////////////////////
const char *vkenv_getVkResultString(VkResult result);

#endif // VKENV_UTILS_H