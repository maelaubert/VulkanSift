#include "vulkan_utils.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char LOG_TAG[] = "VulkanUtils";

////////////////////////////////////////////////////////////////////////
// MISC
////////////////////////////////////////////////////////////////////////
bool vkenv_beginInstantCommandBuffer(VkDevice device, VkCommandPool pool, VkCommandBuffer *command_buffer)
{
  VkResult vkres;
  VkCommandBufferAllocateInfo command_buff_alloc_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                         .pNext = NULL,
                                                         .commandPool = pool,
                                                         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                         .commandBufferCount = 1};
  vkres = vkAllocateCommandBuffers(device, &command_buff_alloc_info, command_buffer);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate command buffer in vkenv_beginInstantCommandBuffer (vkAllocateCommandBuffers: %s)",
             vkenv_getVkResultString(vkres));
    return false;
  }

  // Begin command recording
  VkCommandBufferBeginInfo command_buff_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = NULL, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, .pInheritanceInfo = NULL};
  vkres = vkBeginCommandBuffer(*command_buffer, &command_buff_begin_info);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to begin command buffer in vkenv_beginInstantCommandBuffer (vkBeginCommandBuffer: %s)", vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}
bool vkenv_endInstantCommandBuffer(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer command_buffer)
{
  VkResult vkres = vkEndCommandBuffer(command_buffer);

  // End recording
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to end command buffer in vkenv_endInstantCommandBuffer (vkEndCommandBuffer: %s)", vkenv_getVkResultString(vkres));
    return false;
  }

  // Submit command buffer to queue
  VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = NULL,
                              .waitSemaphoreCount = 0,
                              .pWaitSemaphores = NULL,
                              .pWaitDstStageMask = NULL,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &command_buffer,
                              .signalSemaphoreCount = 0,
                              .pSignalSemaphores = NULL};

  vkres = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to submit command buffer in vkenv_endInstantCommandBuffer (vkQueueSubmit: %s)", vkenv_getVkResultString(vkres));
    return false;
  }

  // Wait for execution to finish
  vkres = vkQueueWaitIdle(queue);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "vkQueueWaitIdle failed in vkenv_endInstantCommandBuffer (vkQueueWaitIdle: %s)", vkenv_getVkResultString(vkres));
    return false;
  }

  // Free command buffer
  vkFreeCommandBuffers(device, pool, 1, &command_buffer);
  return true;
}

////////////////////////////////////////////////////////////////////////
// SHADER
////////////////////////////////////////////////////////////////////////

bool createShaderModuleFromFile(VkDevice device, const char *shader_file_path, VkShaderModule *shader_module)
{
  // Read shader code from file
  FILE *f_shader_file = fopen(shader_file_path, "rb");
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
  int nb_read = fread(shader_code, shader_code_size, 1, f_shader_file);
  if(nb_read != shader_code_size)
  {
    logError(LOG_TAG, "Failed to read shader file %s", shader_file_path);
    return false;
  }
  fclose(f_shader_file);

  VkShaderModuleCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                          .pNext = NULL,
                                          .flags = 0,
                                          .codeSize = (size_t)shader_code_size,
                                          .pCode = (const uint32_t *)shader_code};
  VkResult vkres = vkCreateShaderModule(device, &create_info, NULL, shader_module);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create shader module from file %s (vkCreateShaderModule: %s)", shader_file_path, vkenv_getVkResultString(vkres));
    free(shader_code);
    return false;
  }

  free(shader_code);
  return true;
}

#if defined(VKENV_USE_EMBEDDED_SHADERS)
bool createShaderModuleFromEmbeddedShader(VkDevice device, const char *shader_file_path, VkShaderModule *shader_module)
{
  uint32_t shader_code_size = 0;
  const uint8_t *shader_code = NULL;
  if (!__vkenv_get_embedded_shader_code(shader_file_path, &shader_code_size, &shader_code))
  {
    logError(LOG_TAG, "Failed to find %s in the embedded shaders code", shader_file_path);
    return false;
  }
  VkShaderModuleCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                          .pNext = NULL,
                                          .flags = 0,
                                          .codeSize = (size_t)shader_code_size,
                                          .pCode = (const uint32_t *)shader_code};
  VkResult vkres = vkCreateShaderModule(device, &create_info, NULL, shader_module);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create shader module from file %s (vkCreateShaderModule: %s)", shader_file_path, vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}
#endif

bool vkenv_createShaderModule(VkDevice device, const char *shader_file_path, VkShaderModule *shader_module)
{
#if defined(VKENV_USE_EMBEDDED_SHADERS)
  return createShaderModuleFromEmbeddedShader(device, shader_file_path, shader_module);
#else
  return createShaderModuleFromFile(device, shader_file_path, shader_module);
#endif
}

////////////////////////////////////////////////////////////////////////
// PIPELINES
////////////////////////////////////////////////////////////////////////
bool vkenv_createComputePipeline(VkDevice device, VkShaderModule shader_module, VkDescriptorSetLayout descriptor_set_layout, uint32_t push_constant_size,
                                 VkPipelineLayout *pipeline_layout, VkPipeline *pipeline)
{
  // Setup compute shader module stage
  VkPipelineShaderStageCreateInfo pipeline_shader_stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                           .pNext = NULL,
                                                           .flags = 0,
                                                           .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                           .module = shader_module,
                                                           .pName = "main",
                                                           .pSpecializationInfo = NULL};

  // Setup pipeline layout
  VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                     .pNext = NULL,
                                                     .flags = 0,
                                                     .setLayoutCount = 1,
                                                     .pSetLayouts = &descriptor_set_layout,
                                                     .pushConstantRangeCount = 0,
                                                     .pPushConstantRanges = NULL};
  VkPushConstantRange push_constant_range;
  if (push_constant_size > 0)
  {
    // Set push constant properties if needed
    push_constant_range = (VkPushConstantRange){.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0u, .size = push_constant_size};
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
  }

  VkResult vkres;
  vkres = vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, pipeline_layout);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Pipeline layout creation failed (vkCreatePipelineLayout: %s)", vkenv_getVkResultString(vkres));
    return false;
  }

  // Create pipeline
  VkComputePipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                               .pNext = NULL,
                                               .flags = 0,
                                               .stage = pipeline_shader_stage,
                                               .layout = *pipeline_layout,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1};
  vkres = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, pipeline);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Compute pipeline creation failed (vkCreateComputePipelines: %s)", vkenv_getVkResultString(vkres));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////
// RESOURCES
////////////////////////////////////////////////////////////////////////

bool vkenv_allocateMemory(VkDeviceMemory *memory_ptr, vkenv_Device device, VkDeviceSize size, uint32_t memory_type_idx)
{
  VkMemoryAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = NULL, .allocationSize = size, .memoryTypeIndex = memory_type_idx};
  VkResult vkres = vkAllocateMemory(device->device, &allocInfo, NULL, memory_ptr);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to allocate memory (vkAllocateMemory: %s)", vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}

bool vkenv_findValidMemoryType(VkPhysicalDevice physical_device, VkMemoryRequirements memory_requirements, VkMemoryPropertyFlags memory_properties,
                               uint32_t *memory_type_idx)
{
  VkPhysicalDeviceMemoryProperties available_memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &available_memory_properties);
  bool mem_type_found = false;

  // memory_requirements is a bit set where each bit correspond to a memory type, the bit is set if the memory type support the requirements
  // Here we're looking for a memory type where the corresponding bit in memory_requirements is set and that support the required given memory properties
  for (uint32_t i = 0; i < available_memory_properties.memoryTypeCount; i++)
  {
    if ((memory_requirements.memoryTypeBits & (1 << i)) &&
        (available_memory_properties.memoryTypes[i].propertyFlags & memory_properties) == memory_properties)
    {
      *memory_type_idx = i;
      mem_type_found = true;
      break;
    }
  }
  if (!mem_type_found)
  {
    logError(LOG_TAG, "Failed to find a valid memory type");
  }

  return mem_type_found;
}

////////////////////////////////////////////////////////////////////////
// IMAGES
////////////////////////////////////////////////////////////////////////

bool vkenv_createImage(VkImage *image_ptr, vkenv_Device device, VkImageCreateFlags flags, VkImageType image_type, VkFormat format, VkExtent3D extent,
                       uint32_t mip_levels, uint32_t array_layers, VkSampleCountFlags samples, VkImageTiling tiling, VkImageUsageFlags usage,
                       VkSharingMode sharing_mode, uint32_t queue_family_index_count, const uint32_t *queue_family_indices_ptr,
                       VkImageLayout initial_layout)
{
  // Create vkImage instance
  VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                  .pNext = NULL,
                                  .flags = flags,
                                  .imageType = image_type,
                                  .format = format,
                                  .extent = extent,
                                  .mipLevels = mip_levels,
                                  .arrayLayers = array_layers,
                                  .samples = samples,
                                  .tiling = tiling,
                                  .usage = usage,
                                  .sharingMode = sharing_mode,
                                  .queueFamilyIndexCount = queue_family_index_count,
                                  .pQueueFamilyIndices = queue_family_indices_ptr,
                                  .initialLayout = initial_layout};

  VkResult vkres = vkCreateImage(device->device, &image_info, NULL, image_ptr);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create image (vkCreateImage: %s)", vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}

bool vkenv_createImageView(VkImageView *image_view_ptr, vkenv_Device device, VkImageViewCreateFlags flags, VkImage image, VkImageViewType view_type,
                           VkFormat format, VkComponentMapping components, VkImageSubresourceRange range)
{
  VkImageViewCreateInfo image_view_create_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                  .pNext = NULL,
                                                  .flags = flags,
                                                  .image = image,
                                                  .viewType = view_type,
                                                  .format = format,
                                                  .components = components,
                                                  .subresourceRange = range};

  VkResult vkres = vkCreateImageView(device->device, &image_view_create_info, NULL, image_view_ptr);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create the image view (vkCreateImageView: %s)", vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}

bool vkenv_bindImageMemory(vkenv_Device device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset)
{
  VkResult vkres = vkBindImageMemory(device->device, image, memory, offset);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to bind image memory (vkBindImageMemory: %s)", vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}

VkImageMemoryBarrier vkenv_genImageMemoryBarrier(VkImage image, VkAccessFlags src_mask, VkAccessFlags dst_mask, VkImageLayout old_layout,
                                                 VkImageLayout new_layout, uint32_t src_queue_family_idx, uint32_t dst_queue_family_idx,
                                                 VkImageSubresourceRange range)
{
  VkImageMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                  .pNext = NULL,
                                  .srcAccessMask = src_mask,
                                  .dstAccessMask = dst_mask,
                                  .oldLayout = old_layout,
                                  .newLayout = new_layout,
                                  .srcQueueFamilyIndex = src_queue_family_idx,
                                  .dstQueueFamilyIndex = dst_queue_family_idx,
                                  .image = image,
                                  .subresourceRange = range};
  return barrier;
}

////////////////////////////////////////////////////////////////////////
// BUFFER
////////////////////////////////////////////////////////////////////////

bool vkenv_createBuffer(VkBuffer *buffer_ptr, vkenv_Device device, VkBufferCreateFlags flags, VkDeviceSize size, VkBufferUsageFlags usage,
                        VkSharingMode sharing_mode, uint32_t queue_family_index_count, const uint32_t *queue_family_indices_ptr)
{
  // Create buffer
  VkBufferCreateInfo buff_create_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                         .pNext = NULL,
                                         .flags = flags,
                                         .size = size,
                                         .usage = usage,
                                         .sharingMode = sharing_mode,
                                         .queueFamilyIndexCount = queue_family_index_count,
                                         .pQueueFamilyIndices = queue_family_indices_ptr};
  VkResult vkres = vkCreateBuffer(device->device, &buff_create_info, NULL, buffer_ptr);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create buffer (vkCreateBuffer: %s)", vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}

bool vkenv_bindBufferMemory(vkenv_Device device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset)
{
  VkResult vkres = vkBindBufferMemory(device->device, buffer, memory, offset);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to bind buffer memory (vkBindBufferMemory: %s)", vkenv_getVkResultString(vkres));
    return false;
  }
  return true;
}

VkBufferMemoryBarrier vkenv_genBufferMemoryBarrier(VkBuffer buffer, VkAccessFlags src_mask, VkAccessFlags dst_mask, uint32_t src_queue_family_idx,
                                                   uint32_t dst_queue_family_idx, VkDeviceSize offset, VkDeviceSize size)
{
  VkBufferMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                   .pNext = NULL,
                                   .srcAccessMask = src_mask,
                                   .dstAccessMask = dst_mask,
                                   .srcQueueFamilyIndex = src_queue_family_idx,
                                   .dstQueueFamilyIndex = dst_queue_family_idx,
                                   .buffer = buffer,
                                   .offset = offset,
                                   .size = size};
  return barrier;
}

////////////////////////////////////////////////////////////////////////
// DEBUG UTILS
////////////////////////////////////////////////////////////////////////
const char *vkenv_getVkResultString(VkResult result)
{
  switch (result)
  {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL:
    return "VK_ERROR_FRAGMENTED_POOL";
#if !defined(ANDROID) && !defined(__ANDROID__)
  case VK_ERROR_UNKNOWN:
   return "VK_ERROR_UNKNOWN";
#endif
  default:
    logError(LOG_TAG, "#Unexpected VkResult value: %d#", result);
    return "#Unexpected VkResult value#";
  }
}
////////////////////////////////////////////////////////////////////////
