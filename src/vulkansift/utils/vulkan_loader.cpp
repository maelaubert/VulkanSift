#include "vulkansift/utils/vulkan_loader.h"
#include "vulkansift/utils/logger.h"
#include <dlfcn.h>

static char LOG_TAG[] = "VulkanLoader";

// Instance
PFN_vkCreateInstance vkCreateInstance;
PFN_vkDestroyInstance vkDestroyInstance;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
// Surface
#ifdef VK_USE_PLATFORM_ANDROID_KHR
PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
// Physical device
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
// Logical device
PFN_vkCreateDevice vkCreateDevice;
PFN_vkGetDeviceQueue vkGetDeviceQueue;
PFN_vkDestroyDevice vkDestroyDevice;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
// Swapchain
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
// Image view
PFN_vkCreateImageView vkCreateImageView;
PFN_vkDestroyImageView vkDestroyImageView;
// Render pass
PFN_vkCreateRenderPass vkCreateRenderPass;
PFN_vkDestroyRenderPass vkDestroyRenderPass;
// Framebuffer
PFN_vkCreateFramebuffer vkCreateFramebuffer;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
// Command pool
PFN_vkCreateCommandPool vkCreateCommandPool;
PFN_vkDestroyCommandPool vkDestroyCommandPool;
// Memory
PFN_vkAllocateMemory vkAllocateMemory;
PFN_vkMapMemory vkMapMemory;
PFN_vkUnmapMemory vkUnmapMemory;
PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
PFN_vkFreeMemory vkFreeMemory;
// Buffer
PFN_vkCreateBuffer vkCreateBuffer;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
PFN_vkBindBufferMemory vkBindBufferMemory;
PFN_vkDestroyBuffer vkDestroyBuffer;
// Image
PFN_vkCreateImage vkCreateImage;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
PFN_vkBindImageMemory vkBindImageMemory;
PFN_vkDestroyImage vkDestroyImage;
// Sampler
PFN_vkCreateSampler vkCreateSampler;
PFN_vkDestroySampler vkDestroySampler;
// Command buffer
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
PFN_vkEndCommandBuffer vkEndCommandBuffer;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
PFN_vkResetCommandBuffer vkResetCommandBuffer;
// Command
PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
PFN_vkCmdBindPipeline vkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
PFN_vkCmdDraw vkCmdDraw;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
PFN_vkCmdDispatch vkCmdDispatch;
PFN_vkCmdPushConstants vkCmdPushConstants;
PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
PFN_vkCmdBlitImage vkCmdBlitImage;
PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
PFN_vkCmdFillBuffer vkCmdFillBuffer;
PFN_vkCmdClearColorImage vkCmdClearColorImage;
PFN_vkCmdSetEvent vkCmdSetEvent;
PFN_vkCmdResetEvent vkCmdResetEvent;
PFN_vkCmdWaitEvents vkCmdWaitEvents;
// Events
PFN_vkCreateEvent vkCreateEvent;
PFN_vkDestroyEvent vkDestroyEvent;
PFN_vkSetEvent vkSetEvent;
PFN_vkResetEvent vkResetEvent;
PFN_vkGetEventStatus vkGetEventStatus;
// Queue
PFN_vkQueueSubmit vkQueueSubmit;
PFN_vkQueueWaitIdle vkQueueWaitIdle;
// Descriptors
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
// Shader
PFN_vkCreateShaderModule vkCreateShaderModule;
PFN_vkDestroyShaderModule vkDestroyShaderModule;
// Pipeline
PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines vkCreateComputePipelines;
PFN_vkDestroyPipeline vkDestroyPipeline;
// Synchronisation
PFN_vkCreateSemaphore vkCreateSemaphore;
PFN_vkDestroySemaphore vkDestroySemaphore;
PFN_vkCreateFence vkCreateFence;
PFN_vkDestroyFence vkDestroyFence;
PFN_vkWaitForFences vkWaitForFences;
PFN_vkResetFences vkResetFences;
// KHR
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
PFN_vkQueuePresentKHR vkQueuePresentKHR;
// Query
PFN_vkCreateQueryPool vkCreateQueryPool;
PFN_vkDestroyQueryPool vkDestroyQueryPool;
PFN_vkGetQueryPoolResults vkGetQueryPoolResults;

static void *vulkan_lib_handle = nullptr;

bool loadVulkanSymbol(void *lib_handle, void **symbol_target, const char *symbol_name)
{
  dlerror(); // Clear error code
  bool res = true;
  *symbol_target = dlsym(lib_handle, symbol_name);
  const char *dlsym_error = dlerror();
  if (dlsym_error)
  {
    logError(LOG_TAG, "Failed to load Vulkan symbol (%s)", dlsym_error);
    res = false;
  }
  return res;
}

bool loadVulkan()
{
  // Open Vulkan dynamic library
  vulkan_lib_handle = dlopen("libvulkan.so", RTLD_NOW);
  if (vulkan_lib_handle == nullptr)
  {
    logError(LOG_TAG, "Failed to open libvulkan.so");
    return false;
  }

  // Load Vulkan required symbols
  bool symbol_loading_ok = true;
  // Instance
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateInstance, "vkCreateInstance");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyInstance, "vkDestroyInstance");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkEnumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties");
  // Surface
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateAndroidSurfaceKHR, "vkCreateAndroidSurfaceKHR");
#endif
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroySurfaceKHR, "vkDestroySurfaceKHR");
  // Physical device
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceFeatures, "vkGetPhysicalDeviceFeatures");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkEnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
  symbol_loading_ok &=
      loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR");
  symbol_loading_ok &=
      loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR");
  // Logical device
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateDevice, "vkCreateDevice");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetDeviceQueue, "vkGetDeviceQueue");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyDevice, "vkDestroyDevice");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDeviceWaitIdle, "vkDeviceWaitIdle");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetDeviceProcAddr, "vkGetDeviceProcAddr");
  // Swapchain
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateSwapchainKHR, "vkCreateSwapchainKHR");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroySwapchainKHR, "vkDestroySwapchainKHR");
  // Image view
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateImageView, "vkCreateImageView");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyImageView, "vkDestroyImageView");
  // Render pass
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateRenderPass, "vkCreateRenderPass");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyRenderPass, "vkDestroyRenderPass");
  // Framebuffer
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateFramebuffer, "vkCreateFramebuffer");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyFramebuffer, "vkDestroyFramebuffer");
  // Command pool
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateCommandPool, "vkCreateCommandPool");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyCommandPool, "vkDestroyCommandPool");
  // Memory
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkAllocateMemory, "vkAllocateMemory");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkMapMemory, "vkMapMemory");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkUnmapMemory, "vkUnmapMemory");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkFlushMappedMemoryRanges, "vkFlushMappedMemoryRanges");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkInvalidateMappedMemoryRanges, "vkInvalidateMappedMemoryRanges");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkFreeMemory, "vkFreeMemory");
  // Buffer
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateBuffer, "vkCreateBuffer");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkBindBufferMemory, "vkBindBufferMemory");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyBuffer, "vkDestroyBuffer");
  // Image
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateImage, "vkCreateImage");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkBindImageMemory, "vkBindImageMemory");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyImage, "vkDestroyImage");
  // Sampler
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateSampler, "vkCreateSampler");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroySampler, "vkDestroySampler");
  // Command buffer
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkBeginCommandBuffer, "vkBeginCommandBuffer");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkEndCommandBuffer, "vkEndCommandBuffer");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkFreeCommandBuffers, "vkFreeCommandBuffers");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkResetCommandBuffer, "vkResetCommandBuffer");
  // Command
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdCopyBuffer, "vkCmdCopyBuffer");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdBeginRenderPass, "vkCmdBeginRenderPass");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdEndRenderPass, "vkCmdEndRenderPass");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdBindPipeline, "vkCmdBindPipeline");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdBindIndexBuffer, "vkCmdBindIndexBuffer");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdDraw, "vkCmdDraw");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdDrawIndexed, "vkCmdDrawIndexed");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdCopyBufferToImage, "vkCmdCopyBufferToImage");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdDispatch, "vkCmdDispatch");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdPushConstants, "vkCmdPushConstants");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdWriteTimestamp, "vkCmdWriteTimestamp");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdResetQueryPool, "vkCmdResetQueryPool");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdBlitImage, "vkCmdBlitImage");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdDispatchIndirect, "vkCmdDispatchIndirect");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdFillBuffer, "vkCmdFillBuffer");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdClearColorImage, "vkCmdClearColorImage");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdSetEvent, "vkCmdSetEvent");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdResetEvent, "vkCmdResetEvent");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCmdWaitEvents, "vkCmdWaitEvents");
  // Events
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateEvent, "vkCreateEvent");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyEvent, "vkDestroyEvent");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkSetEvent, "vkSetEvent");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkResetEvent, "vkResetEvent");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetEventStatus, "vkGetEventStatus");
  // Queue
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkQueueSubmit, "vkQueueSubmit");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkQueueWaitIdle, "vkQueueWaitIdle");
  // Descriptors
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateDescriptorPool, "vkCreateDescriptorPool");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
  // Shader
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateShaderModule, "vkCreateShaderModule");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyShaderModule, "vkDestroyShaderModule");
  // Pipeline
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreatePipelineLayout, "vkCreatePipelineLayout");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateComputePipelines, "vkCreateComputePipelines");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyPipeline, "vkDestroyPipeline");
  // Synchronisation
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateSemaphore, "vkCreateSemaphore");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroySemaphore, "vkDestroySemaphore");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateFence, "vkCreateFence");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyFence, "vkDestroyFence");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkWaitForFences, "vkWaitForFences");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkResetFences, "vkResetFences");
  // KHR
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkAcquireNextImageKHR, "vkAcquireNextImageKHR");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkQueuePresentKHR, "vkQueuePresentKHR");
  // Query
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkCreateQueryPool, "vkCreateQueryPool");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkDestroyQueryPool, "vkDestroyQueryPool");
  symbol_loading_ok &= loadVulkanSymbol(vulkan_lib_handle, (void **)&vkGetQueryPoolResults, "vkGetQueryPoolResults");

  // If any error occured when loading symbols
  if (!symbol_loading_ok)
  {
    dlclose(vulkan_lib_handle);
    vulkan_lib_handle = nullptr;
    return false;
  }
  return true;
}

void unloadVulkan()
{
  if (vulkan_lib_handle != nullptr)
  {
    dlclose(vulkan_lib_handle);
  }
}