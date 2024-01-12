#include "vulkan_loader.h"

// Generated for registry version: 1.2.177

#ifndef VK_NO_PROTOTYPES

// Linking done at compile time, nothing to do
bool vkenv_loadVulkanInstanceCreationFuncs() { return true; }
void vkenv_loadVulkanAPI(VkInstance instance) {}
void vkenv_unloadVulkan() {}

#else
// Only if dynamic Vulkan loading requested at compilation
#include "logger.h"
#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#else 
#include <dlfcn.h>
#endif

static char LOG_TAG[] = "VulkanLoader";

#ifdef _WIN32
static HMODULE vulkan_lib_handle;
#else
static void *vulkan_lib_handle = NULL;
#endif

bool loadVkGetInstanceProcAddr()
{
  assert(vulkan_lib_handle == NULL);

#ifdef _WIN32
  vulkan_lib_handle = LoadLibrary("vulkan-1.dll");
  if(vulkan_lib_handle==NULL) {
    logError(LOG_TAG, "Failed to open vulkan-1.dll");
    return false;
  }
  vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkan_lib_handle, "vkGetInstanceProcAddr");
  if(vkGetInstanceProcAddr==NULL) {
    logError(LOG_TAG, "Failed to load Vulkan symbol vkGetInstanceProcAddr");
    return false;
  }
#else
  // Open Vulkan dynamic library
  vulkan_lib_handle = dlopen("libvulkan.so", RTLD_NOW);
  if (vulkan_lib_handle == NULL)
  {
    logError(LOG_TAG, "Failed to open libvulkan.so");
    return false;
  }

  dlerror(); // Clear error code
  vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vulkan_lib_handle, "vkGetInstanceProcAddr");
  const char *dlsym_error = dlerror();
  if (dlsym_error)
  {
    logError(LOG_TAG, "Failed to load Vulkan symbol vkGetInstanceProcAddr (%s)", dlsym_error);
    return false;
  }
#endif
  return true;
}

bool vkenv_loadVulkanInstanceCreationFuncs()
{
  if (!loadVkGetInstanceProcAddr())
  {
    return false;
  }

  // Load Vulkan symbols used to create Vulkan instance
#ifdef VK_VERSION_1_0
  vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(NULL, "vkCreateInstance");
  vkEnumerateInstanceExtensionProperties =
      (PFN_vkEnumerateInstanceExtensionProperties)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceExtensionProperties");
  vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceLayerProperties");
#endif
#ifdef VK_VERSION_1_1
  vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
#endif

  return true;
}

void vkenv_unloadVulkan()
{
  // The only resource to release is the opened .so/.dll file
  if (vulkan_lib_handle != NULL)
  {
#ifdef _WIN32
    FreeLibrary(vulkan_lib_handle);
#else
    dlclose(vulkan_lib_handle);
#endif
    vulkan_lib_handle = NULL;
  }
}

void vkenv_loadVulkanAPI(VkInstance instance)
{
  // Generated code example: "vkCreateInstance  = (PFN_vkCreateInstance)vkGetInstanceProcAddr(instance,"vkCreateInstance");"
  // GENERATED VULKAN API LOADER
#ifdef VK_VERSION_1_0
	vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(instance,"vkCreateInstance");
	vkDestroyInstance = (PFN_vkDestroyInstance)vkGetInstanceProcAddr(instance,"vkDestroyInstance");
	vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)vkGetInstanceProcAddr(instance,"vkEnumeratePhysicalDevices");
	vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures");
	vkGetPhysicalDeviceFormatProperties = (PFN_vkGetPhysicalDeviceFormatProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFormatProperties");
	vkGetPhysicalDeviceImageFormatProperties = (PFN_vkGetPhysicalDeviceImageFormatProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceImageFormatProperties");
	vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties");
	vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceQueueFamilyProperties");
	vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceMemoryProperties");
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr(instance,"vkGetInstanceProcAddr");
	vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(instance,"vkGetDeviceProcAddr");
	vkCreateDevice = (PFN_vkCreateDevice)vkGetInstanceProcAddr(instance,"vkCreateDevice");
	vkDestroyDevice = (PFN_vkDestroyDevice)vkGetInstanceProcAddr(instance,"vkDestroyDevice");
	vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkGetInstanceProcAddr(instance,"vkEnumerateInstanceExtensionProperties");
	vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)vkGetInstanceProcAddr(instance,"vkEnumerateDeviceExtensionProperties");
	vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vkGetInstanceProcAddr(instance,"vkEnumerateInstanceLayerProperties");
	vkEnumerateDeviceLayerProperties = (PFN_vkEnumerateDeviceLayerProperties)vkGetInstanceProcAddr(instance,"vkEnumerateDeviceLayerProperties");
	vkGetDeviceQueue = (PFN_vkGetDeviceQueue)vkGetInstanceProcAddr(instance,"vkGetDeviceQueue");
	vkQueueSubmit = (PFN_vkQueueSubmit)vkGetInstanceProcAddr(instance,"vkQueueSubmit");
	vkQueueWaitIdle = (PFN_vkQueueWaitIdle)vkGetInstanceProcAddr(instance,"vkQueueWaitIdle");
	vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)vkGetInstanceProcAddr(instance,"vkDeviceWaitIdle");
	vkAllocateMemory = (PFN_vkAllocateMemory)vkGetInstanceProcAddr(instance,"vkAllocateMemory");
	vkFreeMemory = (PFN_vkFreeMemory)vkGetInstanceProcAddr(instance,"vkFreeMemory");
	vkMapMemory = (PFN_vkMapMemory)vkGetInstanceProcAddr(instance,"vkMapMemory");
	vkUnmapMemory = (PFN_vkUnmapMemory)vkGetInstanceProcAddr(instance,"vkUnmapMemory");
	vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vkGetInstanceProcAddr(instance,"vkFlushMappedMemoryRanges");
	vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)vkGetInstanceProcAddr(instance,"vkInvalidateMappedMemoryRanges");
	vkGetDeviceMemoryCommitment = (PFN_vkGetDeviceMemoryCommitment)vkGetInstanceProcAddr(instance,"vkGetDeviceMemoryCommitment");
	vkBindBufferMemory = (PFN_vkBindBufferMemory)vkGetInstanceProcAddr(instance,"vkBindBufferMemory");
	vkBindImageMemory = (PFN_vkBindImageMemory)vkGetInstanceProcAddr(instance,"vkBindImageMemory");
	vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vkGetInstanceProcAddr(instance,"vkGetBufferMemoryRequirements");
	vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)vkGetInstanceProcAddr(instance,"vkGetImageMemoryRequirements");
	vkGetImageSparseMemoryRequirements = (PFN_vkGetImageSparseMemoryRequirements)vkGetInstanceProcAddr(instance,"vkGetImageSparseMemoryRequirements");
	vkGetPhysicalDeviceSparseImageFormatProperties = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSparseImageFormatProperties");
	vkQueueBindSparse = (PFN_vkQueueBindSparse)vkGetInstanceProcAddr(instance,"vkQueueBindSparse");
	vkCreateFence = (PFN_vkCreateFence)vkGetInstanceProcAddr(instance,"vkCreateFence");
	vkDestroyFence = (PFN_vkDestroyFence)vkGetInstanceProcAddr(instance,"vkDestroyFence");
	vkResetFences = (PFN_vkResetFences)vkGetInstanceProcAddr(instance,"vkResetFences");
	vkGetFenceStatus = (PFN_vkGetFenceStatus)vkGetInstanceProcAddr(instance,"vkGetFenceStatus");
	vkWaitForFences = (PFN_vkWaitForFences)vkGetInstanceProcAddr(instance,"vkWaitForFences");
	vkCreateSemaphore = (PFN_vkCreateSemaphore)vkGetInstanceProcAddr(instance,"vkCreateSemaphore");
	vkDestroySemaphore = (PFN_vkDestroySemaphore)vkGetInstanceProcAddr(instance,"vkDestroySemaphore");
	vkCreateEvent = (PFN_vkCreateEvent)vkGetInstanceProcAddr(instance,"vkCreateEvent");
	vkDestroyEvent = (PFN_vkDestroyEvent)vkGetInstanceProcAddr(instance,"vkDestroyEvent");
	vkGetEventStatus = (PFN_vkGetEventStatus)vkGetInstanceProcAddr(instance,"vkGetEventStatus");
	vkSetEvent = (PFN_vkSetEvent)vkGetInstanceProcAddr(instance,"vkSetEvent");
	vkResetEvent = (PFN_vkResetEvent)vkGetInstanceProcAddr(instance,"vkResetEvent");
	vkCreateQueryPool = (PFN_vkCreateQueryPool)vkGetInstanceProcAddr(instance,"vkCreateQueryPool");
	vkDestroyQueryPool = (PFN_vkDestroyQueryPool)vkGetInstanceProcAddr(instance,"vkDestroyQueryPool");
	vkGetQueryPoolResults = (PFN_vkGetQueryPoolResults)vkGetInstanceProcAddr(instance,"vkGetQueryPoolResults");
	vkCreateBuffer = (PFN_vkCreateBuffer)vkGetInstanceProcAddr(instance,"vkCreateBuffer");
	vkDestroyBuffer = (PFN_vkDestroyBuffer)vkGetInstanceProcAddr(instance,"vkDestroyBuffer");
	vkCreateBufferView = (PFN_vkCreateBufferView)vkGetInstanceProcAddr(instance,"vkCreateBufferView");
	vkDestroyBufferView = (PFN_vkDestroyBufferView)vkGetInstanceProcAddr(instance,"vkDestroyBufferView");
	vkCreateImage = (PFN_vkCreateImage)vkGetInstanceProcAddr(instance,"vkCreateImage");
	vkDestroyImage = (PFN_vkDestroyImage)vkGetInstanceProcAddr(instance,"vkDestroyImage");
	vkGetImageSubresourceLayout = (PFN_vkGetImageSubresourceLayout)vkGetInstanceProcAddr(instance,"vkGetImageSubresourceLayout");
	vkCreateImageView = (PFN_vkCreateImageView)vkGetInstanceProcAddr(instance,"vkCreateImageView");
	vkDestroyImageView = (PFN_vkDestroyImageView)vkGetInstanceProcAddr(instance,"vkDestroyImageView");
	vkCreateShaderModule = (PFN_vkCreateShaderModule)vkGetInstanceProcAddr(instance,"vkCreateShaderModule");
	vkDestroyShaderModule = (PFN_vkDestroyShaderModule)vkGetInstanceProcAddr(instance,"vkDestroyShaderModule");
	vkCreatePipelineCache = (PFN_vkCreatePipelineCache)vkGetInstanceProcAddr(instance,"vkCreatePipelineCache");
	vkDestroyPipelineCache = (PFN_vkDestroyPipelineCache)vkGetInstanceProcAddr(instance,"vkDestroyPipelineCache");
	vkGetPipelineCacheData = (PFN_vkGetPipelineCacheData)vkGetInstanceProcAddr(instance,"vkGetPipelineCacheData");
	vkMergePipelineCaches = (PFN_vkMergePipelineCaches)vkGetInstanceProcAddr(instance,"vkMergePipelineCaches");
	vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)vkGetInstanceProcAddr(instance,"vkCreateGraphicsPipelines");
	vkCreateComputePipelines = (PFN_vkCreateComputePipelines)vkGetInstanceProcAddr(instance,"vkCreateComputePipelines");
	vkDestroyPipeline = (PFN_vkDestroyPipeline)vkGetInstanceProcAddr(instance,"vkDestroyPipeline");
	vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)vkGetInstanceProcAddr(instance,"vkCreatePipelineLayout");
	vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)vkGetInstanceProcAddr(instance,"vkDestroyPipelineLayout");
	vkCreateSampler = (PFN_vkCreateSampler)vkGetInstanceProcAddr(instance,"vkCreateSampler");
	vkDestroySampler = (PFN_vkDestroySampler)vkGetInstanceProcAddr(instance,"vkDestroySampler");
	vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)vkGetInstanceProcAddr(instance,"vkCreateDescriptorSetLayout");
	vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)vkGetInstanceProcAddr(instance,"vkDestroyDescriptorSetLayout");
	vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)vkGetInstanceProcAddr(instance,"vkCreateDescriptorPool");
	vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)vkGetInstanceProcAddr(instance,"vkDestroyDescriptorPool");
	vkResetDescriptorPool = (PFN_vkResetDescriptorPool)vkGetInstanceProcAddr(instance,"vkResetDescriptorPool");
	vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)vkGetInstanceProcAddr(instance,"vkAllocateDescriptorSets");
	vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets)vkGetInstanceProcAddr(instance,"vkFreeDescriptorSets");
	vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)vkGetInstanceProcAddr(instance,"vkUpdateDescriptorSets");
	vkCreateFramebuffer = (PFN_vkCreateFramebuffer)vkGetInstanceProcAddr(instance,"vkCreateFramebuffer");
	vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)vkGetInstanceProcAddr(instance,"vkDestroyFramebuffer");
	vkCreateRenderPass = (PFN_vkCreateRenderPass)vkGetInstanceProcAddr(instance,"vkCreateRenderPass");
	vkDestroyRenderPass = (PFN_vkDestroyRenderPass)vkGetInstanceProcAddr(instance,"vkDestroyRenderPass");
	vkGetRenderAreaGranularity = (PFN_vkGetRenderAreaGranularity)vkGetInstanceProcAddr(instance,"vkGetRenderAreaGranularity");
	vkCreateCommandPool = (PFN_vkCreateCommandPool)vkGetInstanceProcAddr(instance,"vkCreateCommandPool");
	vkDestroyCommandPool = (PFN_vkDestroyCommandPool)vkGetInstanceProcAddr(instance,"vkDestroyCommandPool");
	vkResetCommandPool = (PFN_vkResetCommandPool)vkGetInstanceProcAddr(instance,"vkResetCommandPool");
	vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)vkGetInstanceProcAddr(instance,"vkAllocateCommandBuffers");
	vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)vkGetInstanceProcAddr(instance,"vkFreeCommandBuffers");
	vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)vkGetInstanceProcAddr(instance,"vkBeginCommandBuffer");
	vkEndCommandBuffer = (PFN_vkEndCommandBuffer)vkGetInstanceProcAddr(instance,"vkEndCommandBuffer");
	vkResetCommandBuffer = (PFN_vkResetCommandBuffer)vkGetInstanceProcAddr(instance,"vkResetCommandBuffer");
	vkCmdBindPipeline = (PFN_vkCmdBindPipeline)vkGetInstanceProcAddr(instance,"vkCmdBindPipeline");
	vkCmdSetViewport = (PFN_vkCmdSetViewport)vkGetInstanceProcAddr(instance,"vkCmdSetViewport");
	vkCmdSetScissor = (PFN_vkCmdSetScissor)vkGetInstanceProcAddr(instance,"vkCmdSetScissor");
	vkCmdSetLineWidth = (PFN_vkCmdSetLineWidth)vkGetInstanceProcAddr(instance,"vkCmdSetLineWidth");
	vkCmdSetDepthBias = (PFN_vkCmdSetDepthBias)vkGetInstanceProcAddr(instance,"vkCmdSetDepthBias");
	vkCmdSetBlendConstants = (PFN_vkCmdSetBlendConstants)vkGetInstanceProcAddr(instance,"vkCmdSetBlendConstants");
	vkCmdSetDepthBounds = (PFN_vkCmdSetDepthBounds)vkGetInstanceProcAddr(instance,"vkCmdSetDepthBounds");
	vkCmdSetStencilCompareMask = (PFN_vkCmdSetStencilCompareMask)vkGetInstanceProcAddr(instance,"vkCmdSetStencilCompareMask");
	vkCmdSetStencilWriteMask = (PFN_vkCmdSetStencilWriteMask)vkGetInstanceProcAddr(instance,"vkCmdSetStencilWriteMask");
	vkCmdSetStencilReference = (PFN_vkCmdSetStencilReference)vkGetInstanceProcAddr(instance,"vkCmdSetStencilReference");
	vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)vkGetInstanceProcAddr(instance,"vkCmdBindDescriptorSets");
	vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)vkGetInstanceProcAddr(instance,"vkCmdBindIndexBuffer");
	vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)vkGetInstanceProcAddr(instance,"vkCmdBindVertexBuffers");
	vkCmdDraw = (PFN_vkCmdDraw)vkGetInstanceProcAddr(instance,"vkCmdDraw");
	vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)vkGetInstanceProcAddr(instance,"vkCmdDrawIndexed");
	vkCmdDrawIndirect = (PFN_vkCmdDrawIndirect)vkGetInstanceProcAddr(instance,"vkCmdDrawIndirect");
	vkCmdDrawIndexedIndirect = (PFN_vkCmdDrawIndexedIndirect)vkGetInstanceProcAddr(instance,"vkCmdDrawIndexedIndirect");
	vkCmdDispatch = (PFN_vkCmdDispatch)vkGetInstanceProcAddr(instance,"vkCmdDispatch");
	vkCmdDispatchIndirect = (PFN_vkCmdDispatchIndirect)vkGetInstanceProcAddr(instance,"vkCmdDispatchIndirect");
	vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)vkGetInstanceProcAddr(instance,"vkCmdCopyBuffer");
	vkCmdCopyImage = (PFN_vkCmdCopyImage)vkGetInstanceProcAddr(instance,"vkCmdCopyImage");
	vkCmdBlitImage = (PFN_vkCmdBlitImage)vkGetInstanceProcAddr(instance,"vkCmdBlitImage");
	vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)vkGetInstanceProcAddr(instance,"vkCmdCopyBufferToImage");
	vkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)vkGetInstanceProcAddr(instance,"vkCmdCopyImageToBuffer");
	vkCmdUpdateBuffer = (PFN_vkCmdUpdateBuffer)vkGetInstanceProcAddr(instance,"vkCmdUpdateBuffer");
	vkCmdFillBuffer = (PFN_vkCmdFillBuffer)vkGetInstanceProcAddr(instance,"vkCmdFillBuffer");
	vkCmdClearColorImage = (PFN_vkCmdClearColorImage)vkGetInstanceProcAddr(instance,"vkCmdClearColorImage");
	vkCmdClearDepthStencilImage = (PFN_vkCmdClearDepthStencilImage)vkGetInstanceProcAddr(instance,"vkCmdClearDepthStencilImage");
	vkCmdClearAttachments = (PFN_vkCmdClearAttachments)vkGetInstanceProcAddr(instance,"vkCmdClearAttachments");
	vkCmdResolveImage = (PFN_vkCmdResolveImage)vkGetInstanceProcAddr(instance,"vkCmdResolveImage");
	vkCmdSetEvent = (PFN_vkCmdSetEvent)vkGetInstanceProcAddr(instance,"vkCmdSetEvent");
	vkCmdResetEvent = (PFN_vkCmdResetEvent)vkGetInstanceProcAddr(instance,"vkCmdResetEvent");
	vkCmdWaitEvents = (PFN_vkCmdWaitEvents)vkGetInstanceProcAddr(instance,"vkCmdWaitEvents");
	vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)vkGetInstanceProcAddr(instance,"vkCmdPipelineBarrier");
	vkCmdBeginQuery = (PFN_vkCmdBeginQuery)vkGetInstanceProcAddr(instance,"vkCmdBeginQuery");
	vkCmdEndQuery = (PFN_vkCmdEndQuery)vkGetInstanceProcAddr(instance,"vkCmdEndQuery");
	vkCmdResetQueryPool = (PFN_vkCmdResetQueryPool)vkGetInstanceProcAddr(instance,"vkCmdResetQueryPool");
	vkCmdWriteTimestamp = (PFN_vkCmdWriteTimestamp)vkGetInstanceProcAddr(instance,"vkCmdWriteTimestamp");
	vkCmdCopyQueryPoolResults = (PFN_vkCmdCopyQueryPoolResults)vkGetInstanceProcAddr(instance,"vkCmdCopyQueryPoolResults");
	vkCmdPushConstants = (PFN_vkCmdPushConstants)vkGetInstanceProcAddr(instance,"vkCmdPushConstants");
	vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)vkGetInstanceProcAddr(instance,"vkCmdBeginRenderPass");
	vkCmdNextSubpass = (PFN_vkCmdNextSubpass)vkGetInstanceProcAddr(instance,"vkCmdNextSubpass");
	vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)vkGetInstanceProcAddr(instance,"vkCmdEndRenderPass");
	vkCmdExecuteCommands = (PFN_vkCmdExecuteCommands)vkGetInstanceProcAddr(instance,"vkCmdExecuteCommands");
#endif

#ifdef VK_VERSION_1_1
	vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(instance,"vkEnumerateInstanceVersion");
	vkBindBufferMemory2 = (PFN_vkBindBufferMemory2)vkGetInstanceProcAddr(instance,"vkBindBufferMemory2");
	vkBindImageMemory2 = (PFN_vkBindImageMemory2)vkGetInstanceProcAddr(instance,"vkBindImageMemory2");
	vkGetDeviceGroupPeerMemoryFeatures = (PFN_vkGetDeviceGroupPeerMemoryFeatures)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupPeerMemoryFeatures");
	vkCmdSetDeviceMask = (PFN_vkCmdSetDeviceMask)vkGetInstanceProcAddr(instance,"vkCmdSetDeviceMask");
	vkCmdDispatchBase = (PFN_vkCmdDispatchBase)vkGetInstanceProcAddr(instance,"vkCmdDispatchBase");
	vkEnumeratePhysicalDeviceGroups = (PFN_vkEnumeratePhysicalDeviceGroups)vkGetInstanceProcAddr(instance,"vkEnumeratePhysicalDeviceGroups");
	vkGetImageMemoryRequirements2 = (PFN_vkGetImageMemoryRequirements2)vkGetInstanceProcAddr(instance,"vkGetImageMemoryRequirements2");
	vkGetBufferMemoryRequirements2 = (PFN_vkGetBufferMemoryRequirements2)vkGetInstanceProcAddr(instance,"vkGetBufferMemoryRequirements2");
	vkGetImageSparseMemoryRequirements2 = (PFN_vkGetImageSparseMemoryRequirements2)vkGetInstanceProcAddr(instance,"vkGetImageSparseMemoryRequirements2");
	vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2");
	vkGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties2");
	vkGetPhysicalDeviceFormatProperties2 = (PFN_vkGetPhysicalDeviceFormatProperties2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFormatProperties2");
	vkGetPhysicalDeviceImageFormatProperties2 = (PFN_vkGetPhysicalDeviceImageFormatProperties2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceImageFormatProperties2");
	vkGetPhysicalDeviceQueueFamilyProperties2 = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceQueueFamilyProperties2");
	vkGetPhysicalDeviceMemoryProperties2 = (PFN_vkGetPhysicalDeviceMemoryProperties2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceMemoryProperties2");
	vkGetPhysicalDeviceSparseImageFormatProperties2 = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSparseImageFormatProperties2");
	vkTrimCommandPool = (PFN_vkTrimCommandPool)vkGetInstanceProcAddr(instance,"vkTrimCommandPool");
	vkGetDeviceQueue2 = (PFN_vkGetDeviceQueue2)vkGetInstanceProcAddr(instance,"vkGetDeviceQueue2");
	vkCreateSamplerYcbcrConversion = (PFN_vkCreateSamplerYcbcrConversion)vkGetInstanceProcAddr(instance,"vkCreateSamplerYcbcrConversion");
	vkDestroySamplerYcbcrConversion = (PFN_vkDestroySamplerYcbcrConversion)vkGetInstanceProcAddr(instance,"vkDestroySamplerYcbcrConversion");
	vkCreateDescriptorUpdateTemplate = (PFN_vkCreateDescriptorUpdateTemplate)vkGetInstanceProcAddr(instance,"vkCreateDescriptorUpdateTemplate");
	vkDestroyDescriptorUpdateTemplate = (PFN_vkDestroyDescriptorUpdateTemplate)vkGetInstanceProcAddr(instance,"vkDestroyDescriptorUpdateTemplate");
	vkUpdateDescriptorSetWithTemplate = (PFN_vkUpdateDescriptorSetWithTemplate)vkGetInstanceProcAddr(instance,"vkUpdateDescriptorSetWithTemplate");
	vkGetPhysicalDeviceExternalBufferProperties = (PFN_vkGetPhysicalDeviceExternalBufferProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceExternalBufferProperties");
	vkGetPhysicalDeviceExternalFenceProperties = (PFN_vkGetPhysicalDeviceExternalFenceProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceExternalFenceProperties");
	vkGetPhysicalDeviceExternalSemaphoreProperties = (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceExternalSemaphoreProperties");
	vkGetDescriptorSetLayoutSupport = (PFN_vkGetDescriptorSetLayoutSupport)vkGetInstanceProcAddr(instance,"vkGetDescriptorSetLayoutSupport");
#endif

#ifdef VK_VERSION_1_2
	vkCmdDrawIndirectCount = (PFN_vkCmdDrawIndirectCount)vkGetInstanceProcAddr(instance,"vkCmdDrawIndirectCount");
	vkCmdDrawIndexedIndirectCount = (PFN_vkCmdDrawIndexedIndirectCount)vkGetInstanceProcAddr(instance,"vkCmdDrawIndexedIndirectCount");
	vkCreateRenderPass2 = (PFN_vkCreateRenderPass2)vkGetInstanceProcAddr(instance,"vkCreateRenderPass2");
	vkCmdBeginRenderPass2 = (PFN_vkCmdBeginRenderPass2)vkGetInstanceProcAddr(instance,"vkCmdBeginRenderPass2");
	vkCmdNextSubpass2 = (PFN_vkCmdNextSubpass2)vkGetInstanceProcAddr(instance,"vkCmdNextSubpass2");
	vkCmdEndRenderPass2 = (PFN_vkCmdEndRenderPass2)vkGetInstanceProcAddr(instance,"vkCmdEndRenderPass2");
	vkResetQueryPool = (PFN_vkResetQueryPool)vkGetInstanceProcAddr(instance,"vkResetQueryPool");
	vkGetSemaphoreCounterValue = (PFN_vkGetSemaphoreCounterValue)vkGetInstanceProcAddr(instance,"vkGetSemaphoreCounterValue");
	vkWaitSemaphores = (PFN_vkWaitSemaphores)vkGetInstanceProcAddr(instance,"vkWaitSemaphores");
	vkSignalSemaphore = (PFN_vkSignalSemaphore)vkGetInstanceProcAddr(instance,"vkSignalSemaphore");
	vkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)vkGetInstanceProcAddr(instance,"vkGetBufferDeviceAddress");
	vkGetBufferOpaqueCaptureAddress = (PFN_vkGetBufferOpaqueCaptureAddress)vkGetInstanceProcAddr(instance,"vkGetBufferOpaqueCaptureAddress");
	vkGetDeviceMemoryOpaqueCaptureAddress = (PFN_vkGetDeviceMemoryOpaqueCaptureAddress)vkGetInstanceProcAddr(instance,"vkGetDeviceMemoryOpaqueCaptureAddress");
#endif

#if defined(VK_KHR_surface)
	vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(instance,"vkDestroySurfaceKHR");
	vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfaceSupportKHR");
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
	vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfaceFormatsKHR");
	vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfacePresentModesKHR");
#endif

#if defined(VK_KHR_swapchain) && defined(VK_KHR_surface)
	vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)vkGetInstanceProcAddr(instance,"vkCreateSwapchainKHR");
	vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)vkGetInstanceProcAddr(instance,"vkDestroySwapchainKHR");
	vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)vkGetInstanceProcAddr(instance,"vkGetSwapchainImagesKHR");
	vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)vkGetInstanceProcAddr(instance,"vkAcquireNextImageKHR");
	vkQueuePresentKHR = (PFN_vkQueuePresentKHR)vkGetInstanceProcAddr(instance,"vkQueuePresentKHR");
#if defined(VK_VERSION_1_1)
	vkGetDeviceGroupPresentCapabilitiesKHR = (PFN_vkGetDeviceGroupPresentCapabilitiesKHR)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupPresentCapabilitiesKHR");
	vkGetDeviceGroupSurfacePresentModesKHR = (PFN_vkGetDeviceGroupSurfacePresentModesKHR)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupSurfacePresentModesKHR");
	vkGetPhysicalDevicePresentRectanglesKHR = (PFN_vkGetPhysicalDevicePresentRectanglesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDevicePresentRectanglesKHR");
	vkAcquireNextImage2KHR = (PFN_vkAcquireNextImage2KHR)vkGetInstanceProcAddr(instance,"vkAcquireNextImage2KHR");
#endif
#endif

#if defined(VK_KHR_display) && defined(VK_KHR_surface)
	vkGetPhysicalDeviceDisplayPropertiesKHR = (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceDisplayPropertiesKHR");
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR = (PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
	vkGetDisplayPlaneSupportedDisplaysKHR = (PFN_vkGetDisplayPlaneSupportedDisplaysKHR)vkGetInstanceProcAddr(instance,"vkGetDisplayPlaneSupportedDisplaysKHR");
	vkGetDisplayModePropertiesKHR = (PFN_vkGetDisplayModePropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetDisplayModePropertiesKHR");
	vkCreateDisplayModeKHR = (PFN_vkCreateDisplayModeKHR)vkGetInstanceProcAddr(instance,"vkCreateDisplayModeKHR");
	vkGetDisplayPlaneCapabilitiesKHR = (PFN_vkGetDisplayPlaneCapabilitiesKHR)vkGetInstanceProcAddr(instance,"vkGetDisplayPlaneCapabilitiesKHR");
	vkCreateDisplayPlaneSurfaceKHR = (PFN_vkCreateDisplayPlaneSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateDisplayPlaneSurfaceKHR");
#endif

#if defined(VK_KHR_display_swapchain) && defined(VK_KHR_swapchain) && defined(VK_KHR_display)
	vkCreateSharedSwapchainsKHR = (PFN_vkCreateSharedSwapchainsKHR)vkGetInstanceProcAddr(instance,"vkCreateSharedSwapchainsKHR");
#endif

#if defined(VK_KHR_xlib_surface) && defined(VK_KHR_surface)
	vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateXlibSurfaceKHR");
	vkGetPhysicalDeviceXlibPresentationSupportKHR = (PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceXlibPresentationSupportKHR");
#endif

#if defined(VK_KHR_xcb_surface) && defined(VK_KHR_surface)
	vkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateXcbSurfaceKHR");
	vkGetPhysicalDeviceXcbPresentationSupportKHR = (PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceXcbPresentationSupportKHR");
#endif

#if defined(VK_KHR_wayland_surface) && defined(VK_KHR_surface)
	vkCreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateWaylandSurfaceKHR");
	vkGetPhysicalDeviceWaylandPresentationSupportKHR = (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceWaylandPresentationSupportKHR");
#endif

#if defined(VK_KHR_android_surface) && defined(VK_KHR_surface)
	vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateAndroidSurfaceKHR");
#endif

#if defined(VK_KHR_win32_surface) && defined(VK_KHR_surface)
	vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateWin32SurfaceKHR");
	vkGetPhysicalDeviceWin32PresentationSupportKHR = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceWin32PresentationSupportKHR");
#endif

#if defined(VK_EXT_debug_report)
	vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugReportCallbackEXT");
	vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugReportCallbackEXT");
	vkDebugReportMessageEXT = (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(instance,"vkDebugReportMessageEXT");
#endif

#if defined(VK_EXT_debug_marker) && defined(VK_EXT_debug_report)
	vkDebugMarkerSetObjectTagEXT = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetInstanceProcAddr(instance,"vkDebugMarkerSetObjectTagEXT");
	vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(instance,"vkDebugMarkerSetObjectNameEXT");
	vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vkGetInstanceProcAddr(instance,"vkCmdDebugMarkerBeginEXT");
	vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vkGetInstanceProcAddr(instance,"vkCmdDebugMarkerEndEXT");
	vkCmdDebugMarkerInsertEXT = (PFN_vkCmdDebugMarkerInsertEXT)vkGetInstanceProcAddr(instance,"vkCmdDebugMarkerInsertEXT");
#endif

#if defined(VK_KHR_video_queue) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_sampler_ycbcr_conversion)
	vkGetPhysicalDeviceVideoCapabilitiesKHR = (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceVideoCapabilitiesKHR");
	vkGetPhysicalDeviceVideoFormatPropertiesKHR = (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceVideoFormatPropertiesKHR");
	vkCreateVideoSessionKHR = (PFN_vkCreateVideoSessionKHR)vkGetInstanceProcAddr(instance,"vkCreateVideoSessionKHR");
	vkDestroyVideoSessionKHR = (PFN_vkDestroyVideoSessionKHR)vkGetInstanceProcAddr(instance,"vkDestroyVideoSessionKHR");
	vkGetVideoSessionMemoryRequirementsKHR = (PFN_vkGetVideoSessionMemoryRequirementsKHR)vkGetInstanceProcAddr(instance,"vkGetVideoSessionMemoryRequirementsKHR");
	vkBindVideoSessionMemoryKHR = (PFN_vkBindVideoSessionMemoryKHR)vkGetInstanceProcAddr(instance,"vkBindVideoSessionMemoryKHR");
	vkCreateVideoSessionParametersKHR = (PFN_vkCreateVideoSessionParametersKHR)vkGetInstanceProcAddr(instance,"vkCreateVideoSessionParametersKHR");
	vkUpdateVideoSessionParametersKHR = (PFN_vkUpdateVideoSessionParametersKHR)vkGetInstanceProcAddr(instance,"vkUpdateVideoSessionParametersKHR");
	vkDestroyVideoSessionParametersKHR = (PFN_vkDestroyVideoSessionParametersKHR)vkGetInstanceProcAddr(instance,"vkDestroyVideoSessionParametersKHR");
	vkCmdBeginVideoCodingKHR = (PFN_vkCmdBeginVideoCodingKHR)vkGetInstanceProcAddr(instance,"vkCmdBeginVideoCodingKHR");
	vkCmdEndVideoCodingKHR = (PFN_vkCmdEndVideoCodingKHR)vkGetInstanceProcAddr(instance,"vkCmdEndVideoCodingKHR");
	vkCmdControlVideoCodingKHR = (PFN_vkCmdControlVideoCodingKHR)vkGetInstanceProcAddr(instance,"vkCmdControlVideoCodingKHR");
#endif

#if defined(VK_KHR_video_decode_queue) && defined(VK_KHR_video_queue) && defined(VK_KHR_synchronization2)
	vkCmdDecodeVideoKHR = (PFN_vkCmdDecodeVideoKHR)vkGetInstanceProcAddr(instance,"vkCmdDecodeVideoKHR");
#endif

#if defined(VK_KHR_video_encode_queue) && defined(VK_KHR_video_queue) && defined(VK_KHR_synchronization2)
	vkCmdEncodeVideoKHR = (PFN_vkCmdEncodeVideoKHR)vkGetInstanceProcAddr(instance,"vkCmdEncodeVideoKHR");
#endif

#if defined(VK_EXT_transform_feedback) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdBindTransformFeedbackBuffersEXT = (PFN_vkCmdBindTransformFeedbackBuffersEXT)vkGetInstanceProcAddr(instance,"vkCmdBindTransformFeedbackBuffersEXT");
	vkCmdBeginTransformFeedbackEXT = (PFN_vkCmdBeginTransformFeedbackEXT)vkGetInstanceProcAddr(instance,"vkCmdBeginTransformFeedbackEXT");
	vkCmdEndTransformFeedbackEXT = (PFN_vkCmdEndTransformFeedbackEXT)vkGetInstanceProcAddr(instance,"vkCmdEndTransformFeedbackEXT");
	vkCmdBeginQueryIndexedEXT = (PFN_vkCmdBeginQueryIndexedEXT)vkGetInstanceProcAddr(instance,"vkCmdBeginQueryIndexedEXT");
	vkCmdEndQueryIndexedEXT = (PFN_vkCmdEndQueryIndexedEXT)vkGetInstanceProcAddr(instance,"vkCmdEndQueryIndexedEXT");
	vkCmdDrawIndirectByteCountEXT = (PFN_vkCmdDrawIndirectByteCountEXT)vkGetInstanceProcAddr(instance,"vkCmdDrawIndirectByteCountEXT");
#endif

#if defined(VK_NVX_image_view_handle)
	vkGetImageViewHandleNVX = (PFN_vkGetImageViewHandleNVX)vkGetInstanceProcAddr(instance,"vkGetImageViewHandleNVX");
	vkGetImageViewAddressNVX = (PFN_vkGetImageViewAddressNVX)vkGetInstanceProcAddr(instance,"vkGetImageViewAddressNVX");
#endif

#if defined(VK_AMD_draw_indirect_count)
	vkCmdDrawIndirectCountAMD = (PFN_vkCmdDrawIndirectCountAMD)vkGetInstanceProcAddr(instance,"vkCmdDrawIndirectCountAMD");
	vkCmdDrawIndexedIndirectCountAMD = (PFN_vkCmdDrawIndexedIndirectCountAMD)vkGetInstanceProcAddr(instance,"vkCmdDrawIndexedIndirectCountAMD");
#endif

#if defined(VK_AMD_shader_info)
	vkGetShaderInfoAMD = (PFN_vkGetShaderInfoAMD)vkGetInstanceProcAddr(instance,"vkGetShaderInfoAMD");
#endif

#if defined(VK_GGP_stream_descriptor_surface) && defined(VK_KHR_surface)
	vkCreateStreamDescriptorSurfaceGGP = (PFN_vkCreateStreamDescriptorSurfaceGGP)vkGetInstanceProcAddr(instance,"vkCreateStreamDescriptorSurfaceGGP");
#endif

#if defined(VK_NV_external_memory_capabilities)
	vkGetPhysicalDeviceExternalImageFormatPropertiesNV = (PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceExternalImageFormatPropertiesNV");
#endif

#if defined(VK_NV_external_memory_win32) && defined(VK_NV_external_memory)
	vkGetMemoryWin32HandleNV = (PFN_vkGetMemoryWin32HandleNV)vkGetInstanceProcAddr(instance,"vkGetMemoryWin32HandleNV");
#endif

#if defined(VK_KHR_get_physical_device_properties2)
	vkGetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2KHR");
	vkGetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties2KHR");
	vkGetPhysicalDeviceFormatProperties2KHR = (PFN_vkGetPhysicalDeviceFormatProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFormatProperties2KHR");
	vkGetPhysicalDeviceImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceImageFormatProperties2KHR");
	vkGetPhysicalDeviceQueueFamilyProperties2KHR = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceQueueFamilyProperties2KHR");
	vkGetPhysicalDeviceMemoryProperties2KHR = (PFN_vkGetPhysicalDeviceMemoryProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceMemoryProperties2KHR");
	vkGetPhysicalDeviceSparseImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
#endif

#if defined(VK_KHR_device_group) && defined(VK_KHR_device_group_creation)
	vkGetDeviceGroupPeerMemoryFeaturesKHR = (PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupPeerMemoryFeaturesKHR");
	vkCmdSetDeviceMaskKHR = (PFN_vkCmdSetDeviceMaskKHR)vkGetInstanceProcAddr(instance,"vkCmdSetDeviceMaskKHR");
	vkCmdDispatchBaseKHR = (PFN_vkCmdDispatchBaseKHR)vkGetInstanceProcAddr(instance,"vkCmdDispatchBaseKHR");
#if defined(VK_KHR_surface)
	vkGetDeviceGroupPresentCapabilitiesKHR = (PFN_vkGetDeviceGroupPresentCapabilitiesKHR)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupPresentCapabilitiesKHR");
	vkGetDeviceGroupSurfacePresentModesKHR = (PFN_vkGetDeviceGroupSurfacePresentModesKHR)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupSurfacePresentModesKHR");
	vkGetPhysicalDevicePresentRectanglesKHR = (PFN_vkGetPhysicalDevicePresentRectanglesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDevicePresentRectanglesKHR");
#endif
#if defined(VK_KHR_swapchain)
	vkAcquireNextImage2KHR = (PFN_vkAcquireNextImage2KHR)vkGetInstanceProcAddr(instance,"vkAcquireNextImage2KHR");
#endif
#endif

#if defined(VK_NN_vi_surface) && defined(VK_KHR_surface)
	vkCreateViSurfaceNN = (PFN_vkCreateViSurfaceNN)vkGetInstanceProcAddr(instance,"vkCreateViSurfaceNN");
#endif

#if defined(VK_KHR_maintenance1)
	vkTrimCommandPoolKHR = (PFN_vkTrimCommandPoolKHR)vkGetInstanceProcAddr(instance,"vkTrimCommandPoolKHR");
#endif

#if defined(VK_KHR_device_group_creation)
	vkEnumeratePhysicalDeviceGroupsKHR = (PFN_vkEnumeratePhysicalDeviceGroupsKHR)vkGetInstanceProcAddr(instance,"vkEnumeratePhysicalDeviceGroupsKHR");
#endif

#if defined(VK_KHR_external_memory_capabilities) && defined(VK_KHR_get_physical_device_properties2)
	vkGetPhysicalDeviceExternalBufferPropertiesKHR = (PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceExternalBufferPropertiesKHR");
#endif

#if defined(VK_KHR_external_memory_win32) && defined(VK_KHR_external_memory)
	vkGetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)vkGetInstanceProcAddr(instance,"vkGetMemoryWin32HandleKHR");
	vkGetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetMemoryWin32HandlePropertiesKHR");
#endif

#if defined(VK_KHR_external_memory_fd) && defined(VK_KHR_external_memory)
	vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)vkGetInstanceProcAddr(instance,"vkGetMemoryFdKHR");
	vkGetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetMemoryFdPropertiesKHR");
#endif

#if defined(VK_KHR_external_semaphore_capabilities) && defined(VK_KHR_get_physical_device_properties2)
	vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = (PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
#endif

#if defined(VK_KHR_external_semaphore_win32) && defined(VK_KHR_external_semaphore)
	vkImportSemaphoreWin32HandleKHR = (PFN_vkImportSemaphoreWin32HandleKHR)vkGetInstanceProcAddr(instance,"vkImportSemaphoreWin32HandleKHR");
	vkGetSemaphoreWin32HandleKHR = (PFN_vkGetSemaphoreWin32HandleKHR)vkGetInstanceProcAddr(instance,"vkGetSemaphoreWin32HandleKHR");
#endif

#if defined(VK_KHR_external_semaphore_fd) && defined(VK_KHR_external_semaphore)
	vkImportSemaphoreFdKHR = (PFN_vkImportSemaphoreFdKHR)vkGetInstanceProcAddr(instance,"vkImportSemaphoreFdKHR");
	vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)vkGetInstanceProcAddr(instance,"vkGetSemaphoreFdKHR");
#endif

#if defined(VK_KHR_push_descriptor) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetInstanceProcAddr(instance,"vkCmdPushDescriptorSetKHR");
#if defined(VK_VERSION_1_1)
	vkCmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)vkGetInstanceProcAddr(instance,"vkCmdPushDescriptorSetWithTemplateKHR");
#endif
#if defined(VK_KHR_descriptor_update_template)
	vkCmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)vkGetInstanceProcAddr(instance,"vkCmdPushDescriptorSetWithTemplateKHR");
#endif
#endif

#if defined(VK_EXT_conditional_rendering)
	vkCmdBeginConditionalRenderingEXT = (PFN_vkCmdBeginConditionalRenderingEXT)vkGetInstanceProcAddr(instance,"vkCmdBeginConditionalRenderingEXT");
	vkCmdEndConditionalRenderingEXT = (PFN_vkCmdEndConditionalRenderingEXT)vkGetInstanceProcAddr(instance,"vkCmdEndConditionalRenderingEXT");
#endif

#if defined(VK_KHR_descriptor_update_template)
	vkCreateDescriptorUpdateTemplateKHR = (PFN_vkCreateDescriptorUpdateTemplateKHR)vkGetInstanceProcAddr(instance,"vkCreateDescriptorUpdateTemplateKHR");
	vkDestroyDescriptorUpdateTemplateKHR = (PFN_vkDestroyDescriptorUpdateTemplateKHR)vkGetInstanceProcAddr(instance,"vkDestroyDescriptorUpdateTemplateKHR");
	vkUpdateDescriptorSetWithTemplateKHR = (PFN_vkUpdateDescriptorSetWithTemplateKHR)vkGetInstanceProcAddr(instance,"vkUpdateDescriptorSetWithTemplateKHR");
#if defined(VK_KHR_push_descriptor)
	vkCmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)vkGetInstanceProcAddr(instance,"vkCmdPushDescriptorSetWithTemplateKHR");
#endif
#endif

#if defined(VK_NV_clip_space_w_scaling)
	vkCmdSetViewportWScalingNV = (PFN_vkCmdSetViewportWScalingNV)vkGetInstanceProcAddr(instance,"vkCmdSetViewportWScalingNV");
#endif

#if defined(VK_EXT_direct_mode_display) && defined(VK_KHR_display)
	vkReleaseDisplayEXT = (PFN_vkReleaseDisplayEXT)vkGetInstanceProcAddr(instance,"vkReleaseDisplayEXT");
#endif

#if defined(VK_EXT_acquire_xlib_display) && defined(VK_EXT_direct_mode_display)
	vkAcquireXlibDisplayEXT = (PFN_vkAcquireXlibDisplayEXT)vkGetInstanceProcAddr(instance,"vkAcquireXlibDisplayEXT");
	vkGetRandROutputDisplayEXT = (PFN_vkGetRandROutputDisplayEXT)vkGetInstanceProcAddr(instance,"vkGetRandROutputDisplayEXT");
#endif

#if defined(VK_EXT_display_surface_counter) && defined(VK_KHR_display)
	vkGetPhysicalDeviceSurfaceCapabilities2EXT = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfaceCapabilities2EXT");
#endif

#if defined(VK_EXT_display_control) && defined(VK_EXT_display_surface_counter) && defined(VK_KHR_swapchain)
	vkDisplayPowerControlEXT = (PFN_vkDisplayPowerControlEXT)vkGetInstanceProcAddr(instance,"vkDisplayPowerControlEXT");
	vkRegisterDeviceEventEXT = (PFN_vkRegisterDeviceEventEXT)vkGetInstanceProcAddr(instance,"vkRegisterDeviceEventEXT");
	vkRegisterDisplayEventEXT = (PFN_vkRegisterDisplayEventEXT)vkGetInstanceProcAddr(instance,"vkRegisterDisplayEventEXT");
	vkGetSwapchainCounterEXT = (PFN_vkGetSwapchainCounterEXT)vkGetInstanceProcAddr(instance,"vkGetSwapchainCounterEXT");
#endif

#if defined(VK_GOOGLE_display_timing) && defined(VK_KHR_swapchain)
	vkGetRefreshCycleDurationGOOGLE = (PFN_vkGetRefreshCycleDurationGOOGLE)vkGetInstanceProcAddr(instance,"vkGetRefreshCycleDurationGOOGLE");
	vkGetPastPresentationTimingGOOGLE = (PFN_vkGetPastPresentationTimingGOOGLE)vkGetInstanceProcAddr(instance,"vkGetPastPresentationTimingGOOGLE");
#endif

#if defined(VK_EXT_discard_rectangles) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetDiscardRectangleEXT = (PFN_vkCmdSetDiscardRectangleEXT)vkGetInstanceProcAddr(instance,"vkCmdSetDiscardRectangleEXT");
#endif

#if defined(VK_EXT_hdr_metadata) && defined(VK_KHR_swapchain)
	vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT)vkGetInstanceProcAddr(instance,"vkSetHdrMetadataEXT");
#endif

#if defined(VK_KHR_create_renderpass2) && defined(VK_KHR_multiview) && defined(VK_KHR_maintenance2)
	vkCreateRenderPass2KHR = (PFN_vkCreateRenderPass2KHR)vkGetInstanceProcAddr(instance,"vkCreateRenderPass2KHR");
	vkCmdBeginRenderPass2KHR = (PFN_vkCmdBeginRenderPass2KHR)vkGetInstanceProcAddr(instance,"vkCmdBeginRenderPass2KHR");
	vkCmdNextSubpass2KHR = (PFN_vkCmdNextSubpass2KHR)vkGetInstanceProcAddr(instance,"vkCmdNextSubpass2KHR");
	vkCmdEndRenderPass2KHR = (PFN_vkCmdEndRenderPass2KHR)vkGetInstanceProcAddr(instance,"vkCmdEndRenderPass2KHR");
#endif

#if defined(VK_KHR_shared_presentable_image) && defined(VK_KHR_swapchain) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_get_surface_capabilities2)
	vkGetSwapchainStatusKHR = (PFN_vkGetSwapchainStatusKHR)vkGetInstanceProcAddr(instance,"vkGetSwapchainStatusKHR");
#endif

#if defined(VK_KHR_external_fence_capabilities) && defined(VK_KHR_get_physical_device_properties2)
	vkGetPhysicalDeviceExternalFencePropertiesKHR = (PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceExternalFencePropertiesKHR");
#endif

#if defined(VK_KHR_external_fence_win32) && defined(VK_KHR_external_fence)
	vkImportFenceWin32HandleKHR = (PFN_vkImportFenceWin32HandleKHR)vkGetInstanceProcAddr(instance,"vkImportFenceWin32HandleKHR");
	vkGetFenceWin32HandleKHR = (PFN_vkGetFenceWin32HandleKHR)vkGetInstanceProcAddr(instance,"vkGetFenceWin32HandleKHR");
#endif

#if defined(VK_KHR_external_fence_fd) && defined(VK_KHR_external_fence)
	vkImportFenceFdKHR = (PFN_vkImportFenceFdKHR)vkGetInstanceProcAddr(instance,"vkImportFenceFdKHR");
	vkGetFenceFdKHR = (PFN_vkGetFenceFdKHR)vkGetInstanceProcAddr(instance,"vkGetFenceFdKHR");
#endif

#if defined(VK_KHR_performance_query) && defined(VK_KHR_get_physical_device_properties2)
	vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR = (PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)vkGetInstanceProcAddr(instance,"vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");
	vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR = (PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");
	vkAcquireProfilingLockKHR = (PFN_vkAcquireProfilingLockKHR)vkGetInstanceProcAddr(instance,"vkAcquireProfilingLockKHR");
	vkReleaseProfilingLockKHR = (PFN_vkReleaseProfilingLockKHR)vkGetInstanceProcAddr(instance,"vkReleaseProfilingLockKHR");
#endif

#if defined(VK_KHR_get_surface_capabilities2) && defined(VK_KHR_surface)
	vkGetPhysicalDeviceSurfaceCapabilities2KHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfaceCapabilities2KHR");
	vkGetPhysicalDeviceSurfaceFormats2KHR = (PFN_vkGetPhysicalDeviceSurfaceFormats2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfaceFormats2KHR");
#endif

#if defined(VK_KHR_get_display_properties2) && defined(VK_KHR_display)
	vkGetPhysicalDeviceDisplayProperties2KHR = (PFN_vkGetPhysicalDeviceDisplayProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceDisplayProperties2KHR");
	vkGetPhysicalDeviceDisplayPlaneProperties2KHR = (PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceDisplayPlaneProperties2KHR");
	vkGetDisplayModeProperties2KHR = (PFN_vkGetDisplayModeProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetDisplayModeProperties2KHR");
	vkGetDisplayPlaneCapabilities2KHR = (PFN_vkGetDisplayPlaneCapabilities2KHR)vkGetInstanceProcAddr(instance,"vkGetDisplayPlaneCapabilities2KHR");
#endif

#if defined(VK_MVK_ios_surface) && defined(VK_KHR_surface)
	vkCreateIOSSurfaceMVK = (PFN_vkCreateIOSSurfaceMVK)vkGetInstanceProcAddr(instance,"vkCreateIOSSurfaceMVK");
#endif

#if defined(VK_MVK_macos_surface) && defined(VK_KHR_surface)
	vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(instance,"vkCreateMacOSSurfaceMVK");
#endif

#if defined(VK_EXT_debug_utils)
	vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance,"vkSetDebugUtilsObjectNameEXT");
	vkSetDebugUtilsObjectTagEXT = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetInstanceProcAddr(instance,"vkSetDebugUtilsObjectTagEXT");
	vkQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance,"vkQueueBeginDebugUtilsLabelEXT");
	vkQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance,"vkQueueEndDebugUtilsLabelEXT");
	vkQueueInsertDebugUtilsLabelEXT = (PFN_vkQueueInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance,"vkQueueInsertDebugUtilsLabelEXT");
	vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance,"vkCmdBeginDebugUtilsLabelEXT");
	vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance,"vkCmdEndDebugUtilsLabelEXT");
	vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance,"vkCmdInsertDebugUtilsLabelEXT");
	vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
	vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
	vkSubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(instance,"vkSubmitDebugUtilsMessageEXT");
#endif

#if defined(VK_ANDROID_external_memory_android_hardware_buffer) && defined(VK_KHR_sampler_ycbcr_conversion) && defined(VK_KHR_external_memory) && defined(VK_EXT_queue_family_foreign) && defined(VK_KHR_dedicated_allocation)
	vkGetAndroidHardwareBufferPropertiesANDROID = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)vkGetInstanceProcAddr(instance,"vkGetAndroidHardwareBufferPropertiesANDROID");
	vkGetMemoryAndroidHardwareBufferANDROID = (PFN_vkGetMemoryAndroidHardwareBufferANDROID)vkGetInstanceProcAddr(instance,"vkGetMemoryAndroidHardwareBufferANDROID");
#endif

#if defined(VK_EXT_sample_locations) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetSampleLocationsEXT = (PFN_vkCmdSetSampleLocationsEXT)vkGetInstanceProcAddr(instance,"vkCmdSetSampleLocationsEXT");
	vkGetPhysicalDeviceMultisamplePropertiesEXT = (PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceMultisamplePropertiesEXT");
#endif

#if defined(VK_KHR_get_memory_requirements2)
	vkGetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2KHR)vkGetInstanceProcAddr(instance,"vkGetImageMemoryRequirements2KHR");
	vkGetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)vkGetInstanceProcAddr(instance,"vkGetBufferMemoryRequirements2KHR");
	vkGetImageSparseMemoryRequirements2KHR = (PFN_vkGetImageSparseMemoryRequirements2KHR)vkGetInstanceProcAddr(instance,"vkGetImageSparseMemoryRequirements2KHR");
#endif

#if defined(VK_KHR_acceleration_structure) && defined(VK_EXT_descriptor_indexing) && defined(VK_KHR_buffer_device_address) && defined(VK_KHR_deferred_host_operations)
	vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetInstanceProcAddr(instance,"vkCreateAccelerationStructureKHR");
	vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetInstanceProcAddr(instance,"vkDestroyAccelerationStructureKHR");
	vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(instance,"vkCmdBuildAccelerationStructuresKHR");
	vkCmdBuildAccelerationStructuresIndirectKHR = (PFN_vkCmdBuildAccelerationStructuresIndirectKHR)vkGetInstanceProcAddr(instance,"vkCmdBuildAccelerationStructuresIndirectKHR");
	vkBuildAccelerationStructuresKHR = (PFN_vkBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(instance,"vkBuildAccelerationStructuresKHR");
	vkCopyAccelerationStructureKHR = (PFN_vkCopyAccelerationStructureKHR)vkGetInstanceProcAddr(instance,"vkCopyAccelerationStructureKHR");
	vkCopyAccelerationStructureToMemoryKHR = (PFN_vkCopyAccelerationStructureToMemoryKHR)vkGetInstanceProcAddr(instance,"vkCopyAccelerationStructureToMemoryKHR");
	vkCopyMemoryToAccelerationStructureKHR = (PFN_vkCopyMemoryToAccelerationStructureKHR)vkGetInstanceProcAddr(instance,"vkCopyMemoryToAccelerationStructureKHR");
	vkWriteAccelerationStructuresPropertiesKHR = (PFN_vkWriteAccelerationStructuresPropertiesKHR)vkGetInstanceProcAddr(instance,"vkWriteAccelerationStructuresPropertiesKHR");
	vkCmdCopyAccelerationStructureKHR = (PFN_vkCmdCopyAccelerationStructureKHR)vkGetInstanceProcAddr(instance,"vkCmdCopyAccelerationStructureKHR");
	vkCmdCopyAccelerationStructureToMemoryKHR = (PFN_vkCmdCopyAccelerationStructureToMemoryKHR)vkGetInstanceProcAddr(instance,"vkCmdCopyAccelerationStructureToMemoryKHR");
	vkCmdCopyMemoryToAccelerationStructureKHR = (PFN_vkCmdCopyMemoryToAccelerationStructureKHR)vkGetInstanceProcAddr(instance,"vkCmdCopyMemoryToAccelerationStructureKHR");
	vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetInstanceProcAddr(instance,"vkGetAccelerationStructureDeviceAddressKHR");
	vkCmdWriteAccelerationStructuresPropertiesKHR = (PFN_vkCmdWriteAccelerationStructuresPropertiesKHR)vkGetInstanceProcAddr(instance,"vkCmdWriteAccelerationStructuresPropertiesKHR");
	vkGetDeviceAccelerationStructureCompatibilityKHR = (PFN_vkGetDeviceAccelerationStructureCompatibilityKHR)vkGetInstanceProcAddr(instance,"vkGetDeviceAccelerationStructureCompatibilityKHR");
	vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetInstanceProcAddr(instance,"vkGetAccelerationStructureBuildSizesKHR");
#endif

#if defined(VK_KHR_ray_tracing_pipeline) && defined(VK_KHR_spirv_1_4) && defined(VK_KHR_acceleration_structure)
	vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetInstanceProcAddr(instance,"vkCmdTraceRaysKHR");
	vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(instance,"vkCreateRayTracingPipelinesKHR");
	vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(instance,"vkGetRayTracingShaderGroupHandlesKHR");
	vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = (PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)vkGetInstanceProcAddr(instance,"vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
	vkCmdTraceRaysIndirectKHR = (PFN_vkCmdTraceRaysIndirectKHR)vkGetInstanceProcAddr(instance,"vkCmdTraceRaysIndirectKHR");
	vkGetRayTracingShaderGroupStackSizeKHR = (PFN_vkGetRayTracingShaderGroupStackSizeKHR)vkGetInstanceProcAddr(instance,"vkGetRayTracingShaderGroupStackSizeKHR");
	vkCmdSetRayTracingPipelineStackSizeKHR = (PFN_vkCmdSetRayTracingPipelineStackSizeKHR)vkGetInstanceProcAddr(instance,"vkCmdSetRayTracingPipelineStackSizeKHR");
#endif

#if defined(VK_KHR_sampler_ycbcr_conversion) && defined(VK_KHR_maintenance1) && defined(VK_KHR_bind_memory2) && defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_get_physical_device_properties2)
	vkCreateSamplerYcbcrConversionKHR = (PFN_vkCreateSamplerYcbcrConversionKHR)vkGetInstanceProcAddr(instance,"vkCreateSamplerYcbcrConversionKHR");
	vkDestroySamplerYcbcrConversionKHR = (PFN_vkDestroySamplerYcbcrConversionKHR)vkGetInstanceProcAddr(instance,"vkDestroySamplerYcbcrConversionKHR");
#endif

#if defined(VK_KHR_bind_memory2)
	vkBindBufferMemory2KHR = (PFN_vkBindBufferMemory2KHR)vkGetInstanceProcAddr(instance,"vkBindBufferMemory2KHR");
	vkBindImageMemory2KHR = (PFN_vkBindImageMemory2KHR)vkGetInstanceProcAddr(instance,"vkBindImageMemory2KHR");
#endif

#if defined(VK_EXT_image_drm_format_modifier) && defined(VK_KHR_bind_memory2) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_image_format_list) && defined(VK_KHR_sampler_ycbcr_conversion)
	vkGetImageDrmFormatModifierPropertiesEXT = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)vkGetInstanceProcAddr(instance,"vkGetImageDrmFormatModifierPropertiesEXT");
#endif

#if defined(VK_EXT_validation_cache)
	vkCreateValidationCacheEXT = (PFN_vkCreateValidationCacheEXT)vkGetInstanceProcAddr(instance,"vkCreateValidationCacheEXT");
	vkDestroyValidationCacheEXT = (PFN_vkDestroyValidationCacheEXT)vkGetInstanceProcAddr(instance,"vkDestroyValidationCacheEXT");
	vkMergeValidationCachesEXT = (PFN_vkMergeValidationCachesEXT)vkGetInstanceProcAddr(instance,"vkMergeValidationCachesEXT");
	vkGetValidationCacheDataEXT = (PFN_vkGetValidationCacheDataEXT)vkGetInstanceProcAddr(instance,"vkGetValidationCacheDataEXT");
#endif

#if defined(VK_NV_shading_rate_image) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdBindShadingRateImageNV = (PFN_vkCmdBindShadingRateImageNV)vkGetInstanceProcAddr(instance,"vkCmdBindShadingRateImageNV");
	vkCmdSetViewportShadingRatePaletteNV = (PFN_vkCmdSetViewportShadingRatePaletteNV)vkGetInstanceProcAddr(instance,"vkCmdSetViewportShadingRatePaletteNV");
	vkCmdSetCoarseSampleOrderNV = (PFN_vkCmdSetCoarseSampleOrderNV)vkGetInstanceProcAddr(instance,"vkCmdSetCoarseSampleOrderNV");
#endif

#if defined(VK_NV_ray_tracing) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_get_memory_requirements2)
	vkCreateAccelerationStructureNV = (PFN_vkCreateAccelerationStructureNV)vkGetInstanceProcAddr(instance,"vkCreateAccelerationStructureNV");
	vkDestroyAccelerationStructureNV = (PFN_vkDestroyAccelerationStructureNV)vkGetInstanceProcAddr(instance,"vkDestroyAccelerationStructureNV");
	vkGetAccelerationStructureMemoryRequirementsNV = (PFN_vkGetAccelerationStructureMemoryRequirementsNV)vkGetInstanceProcAddr(instance,"vkGetAccelerationStructureMemoryRequirementsNV");
	vkBindAccelerationStructureMemoryNV = (PFN_vkBindAccelerationStructureMemoryNV)vkGetInstanceProcAddr(instance,"vkBindAccelerationStructureMemoryNV");
	vkCmdBuildAccelerationStructureNV = (PFN_vkCmdBuildAccelerationStructureNV)vkGetInstanceProcAddr(instance,"vkCmdBuildAccelerationStructureNV");
	vkCmdCopyAccelerationStructureNV = (PFN_vkCmdCopyAccelerationStructureNV)vkGetInstanceProcAddr(instance,"vkCmdCopyAccelerationStructureNV");
	vkCmdTraceRaysNV = (PFN_vkCmdTraceRaysNV)vkGetInstanceProcAddr(instance,"vkCmdTraceRaysNV");
	vkCreateRayTracingPipelinesNV = (PFN_vkCreateRayTracingPipelinesNV)vkGetInstanceProcAddr(instance,"vkCreateRayTracingPipelinesNV");
	vkGetRayTracingShaderGroupHandlesNV = (PFN_vkGetRayTracingShaderGroupHandlesNV)vkGetInstanceProcAddr(instance,"vkGetRayTracingShaderGroupHandlesNV");
	vkGetAccelerationStructureHandleNV = (PFN_vkGetAccelerationStructureHandleNV)vkGetInstanceProcAddr(instance,"vkGetAccelerationStructureHandleNV");
	vkCmdWriteAccelerationStructuresPropertiesNV = (PFN_vkCmdWriteAccelerationStructuresPropertiesNV)vkGetInstanceProcAddr(instance,"vkCmdWriteAccelerationStructuresPropertiesNV");
	vkCompileDeferredNV = (PFN_vkCompileDeferredNV)vkGetInstanceProcAddr(instance,"vkCompileDeferredNV");
#endif

#if defined(VK_KHR_maintenance3) && defined(VK_KHR_get_physical_device_properties2)
	vkGetDescriptorSetLayoutSupportKHR = (PFN_vkGetDescriptorSetLayoutSupportKHR)vkGetInstanceProcAddr(instance,"vkGetDescriptorSetLayoutSupportKHR");
#endif

#if defined(VK_KHR_draw_indirect_count)
	vkCmdDrawIndirectCountKHR = (PFN_vkCmdDrawIndirectCountKHR)vkGetInstanceProcAddr(instance,"vkCmdDrawIndirectCountKHR");
	vkCmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR)vkGetInstanceProcAddr(instance,"vkCmdDrawIndexedIndirectCountKHR");
#endif

#if defined(VK_EXT_external_memory_host) && defined(VK_KHR_external_memory)
	vkGetMemoryHostPointerPropertiesEXT = (PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetInstanceProcAddr(instance,"vkGetMemoryHostPointerPropertiesEXT");
#endif

#if defined(VK_AMD_buffer_marker)
	vkCmdWriteBufferMarkerAMD = (PFN_vkCmdWriteBufferMarkerAMD)vkGetInstanceProcAddr(instance,"vkCmdWriteBufferMarkerAMD");
#endif

#if defined(VK_EXT_calibrated_timestamps)
	vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
	vkGetCalibratedTimestampsEXT = (PFN_vkGetCalibratedTimestampsEXT)vkGetInstanceProcAddr(instance,"vkGetCalibratedTimestampsEXT");
#endif

#if defined(VK_NV_mesh_shader) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdDrawMeshTasksNV = (PFN_vkCmdDrawMeshTasksNV)vkGetInstanceProcAddr(instance,"vkCmdDrawMeshTasksNV");
	vkCmdDrawMeshTasksIndirectNV = (PFN_vkCmdDrawMeshTasksIndirectNV)vkGetInstanceProcAddr(instance,"vkCmdDrawMeshTasksIndirectNV");
	vkCmdDrawMeshTasksIndirectCountNV = (PFN_vkCmdDrawMeshTasksIndirectCountNV)vkGetInstanceProcAddr(instance,"vkCmdDrawMeshTasksIndirectCountNV");
#endif

#if defined(VK_NV_scissor_exclusive) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetExclusiveScissorNV = (PFN_vkCmdSetExclusiveScissorNV)vkGetInstanceProcAddr(instance,"vkCmdSetExclusiveScissorNV");
#endif

#if defined(VK_NV_device_diagnostic_checkpoints) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetCheckpointNV = (PFN_vkCmdSetCheckpointNV)vkGetInstanceProcAddr(instance,"vkCmdSetCheckpointNV");
	vkGetQueueCheckpointDataNV = (PFN_vkGetQueueCheckpointDataNV)vkGetInstanceProcAddr(instance,"vkGetQueueCheckpointDataNV");
#endif

#if defined(VK_KHR_timeline_semaphore) && defined(VK_KHR_get_physical_device_properties2)
	vkGetSemaphoreCounterValueKHR = (PFN_vkGetSemaphoreCounterValueKHR)vkGetInstanceProcAddr(instance,"vkGetSemaphoreCounterValueKHR");
	vkWaitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR)vkGetInstanceProcAddr(instance,"vkWaitSemaphoresKHR");
	vkSignalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR)vkGetInstanceProcAddr(instance,"vkSignalSemaphoreKHR");
#endif

#if defined(VK_INTEL_performance_query)
	vkInitializePerformanceApiINTEL = (PFN_vkInitializePerformanceApiINTEL)vkGetInstanceProcAddr(instance,"vkInitializePerformanceApiINTEL");
	vkUninitializePerformanceApiINTEL = (PFN_vkUninitializePerformanceApiINTEL)vkGetInstanceProcAddr(instance,"vkUninitializePerformanceApiINTEL");
	vkCmdSetPerformanceMarkerINTEL = (PFN_vkCmdSetPerformanceMarkerINTEL)vkGetInstanceProcAddr(instance,"vkCmdSetPerformanceMarkerINTEL");
	vkCmdSetPerformanceStreamMarkerINTEL = (PFN_vkCmdSetPerformanceStreamMarkerINTEL)vkGetInstanceProcAddr(instance,"vkCmdSetPerformanceStreamMarkerINTEL");
	vkCmdSetPerformanceOverrideINTEL = (PFN_vkCmdSetPerformanceOverrideINTEL)vkGetInstanceProcAddr(instance,"vkCmdSetPerformanceOverrideINTEL");
	vkAcquirePerformanceConfigurationINTEL = (PFN_vkAcquirePerformanceConfigurationINTEL)vkGetInstanceProcAddr(instance,"vkAcquirePerformanceConfigurationINTEL");
	vkReleasePerformanceConfigurationINTEL = (PFN_vkReleasePerformanceConfigurationINTEL)vkGetInstanceProcAddr(instance,"vkReleasePerformanceConfigurationINTEL");
	vkQueueSetPerformanceConfigurationINTEL = (PFN_vkQueueSetPerformanceConfigurationINTEL)vkGetInstanceProcAddr(instance,"vkQueueSetPerformanceConfigurationINTEL");
	vkGetPerformanceParameterINTEL = (PFN_vkGetPerformanceParameterINTEL)vkGetInstanceProcAddr(instance,"vkGetPerformanceParameterINTEL");
#endif

#if defined(VK_AMD_display_native_hdr) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_get_surface_capabilities2) && defined(VK_KHR_swapchain)
	vkSetLocalDimmingAMD = (PFN_vkSetLocalDimmingAMD)vkGetInstanceProcAddr(instance,"vkSetLocalDimmingAMD");
#endif

#if defined(VK_FUCHSIA_imagepipe_surface) && defined(VK_KHR_surface)
	vkCreateImagePipeSurfaceFUCHSIA = (PFN_vkCreateImagePipeSurfaceFUCHSIA)vkGetInstanceProcAddr(instance,"vkCreateImagePipeSurfaceFUCHSIA");
#endif

#if defined(VK_EXT_metal_surface) && defined(VK_KHR_surface)
	vkCreateMetalSurfaceEXT = (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(instance,"vkCreateMetalSurfaceEXT");
#endif

#if defined(VK_KHR_fragment_shading_rate) && defined(VK_KHR_create_renderpass2) && defined(VK_KHR_get_physical_device_properties2)
	vkGetPhysicalDeviceFragmentShadingRatesKHR = (PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFragmentShadingRatesKHR");
	vkCmdSetFragmentShadingRateKHR = (PFN_vkCmdSetFragmentShadingRateKHR)vkGetInstanceProcAddr(instance,"vkCmdSetFragmentShadingRateKHR");
#endif

#if defined(VK_EXT_buffer_device_address) && defined(VK_KHR_get_physical_device_properties2)
	vkGetBufferDeviceAddressEXT = (PFN_vkGetBufferDeviceAddressEXT)vkGetInstanceProcAddr(instance,"vkGetBufferDeviceAddressEXT");
#endif

#if defined(VK_EXT_tooling_info)
	vkGetPhysicalDeviceToolPropertiesEXT = (PFN_vkGetPhysicalDeviceToolPropertiesEXT)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceToolPropertiesEXT");
#endif

#if defined(VK_NV_cooperative_matrix) && defined(VK_KHR_get_physical_device_properties2)
	vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = (PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceCooperativeMatrixPropertiesNV");
#endif

#if defined(VK_NV_coverage_reduction_mode) && defined(VK_NV_framebuffer_mixed_samples)
	vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV = (PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
#endif

#if defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_surface) && defined(VK_KHR_get_surface_capabilities2) && defined(VK_KHR_swapchain)
	vkGetPhysicalDeviceSurfacePresentModes2EXT = (PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceSurfacePresentModes2EXT");
	vkAcquireFullScreenExclusiveModeEXT = (PFN_vkAcquireFullScreenExclusiveModeEXT)vkGetInstanceProcAddr(instance,"vkAcquireFullScreenExclusiveModeEXT");
	vkReleaseFullScreenExclusiveModeEXT = (PFN_vkReleaseFullScreenExclusiveModeEXT)vkGetInstanceProcAddr(instance,"vkReleaseFullScreenExclusiveModeEXT");
#if defined(VK_KHR_device_group)
	vkGetDeviceGroupSurfacePresentModes2EXT = (PFN_vkGetDeviceGroupSurfacePresentModes2EXT)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupSurfacePresentModes2EXT");
#endif
#if defined(VK_VERSION_1_1)
	vkGetDeviceGroupSurfacePresentModes2EXT = (PFN_vkGetDeviceGroupSurfacePresentModes2EXT)vkGetInstanceProcAddr(instance,"vkGetDeviceGroupSurfacePresentModes2EXT");
#endif
#endif

#if defined(VK_EXT_headless_surface) && defined(VK_KHR_surface)
	vkCreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)vkGetInstanceProcAddr(instance,"vkCreateHeadlessSurfaceEXT");
#endif

#if defined(VK_KHR_buffer_device_address) && defined(VK_KHR_get_physical_device_properties2)
	vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetInstanceProcAddr(instance,"vkGetBufferDeviceAddressKHR");
	vkGetBufferOpaqueCaptureAddressKHR = (PFN_vkGetBufferOpaqueCaptureAddressKHR)vkGetInstanceProcAddr(instance,"vkGetBufferOpaqueCaptureAddressKHR");
	vkGetDeviceMemoryOpaqueCaptureAddressKHR = (PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR)vkGetInstanceProcAddr(instance,"vkGetDeviceMemoryOpaqueCaptureAddressKHR");
#endif

#if defined(VK_EXT_line_rasterization) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetLineStippleEXT = (PFN_vkCmdSetLineStippleEXT)vkGetInstanceProcAddr(instance,"vkCmdSetLineStippleEXT");
#endif

#if defined(VK_EXT_host_query_reset) && defined(VK_KHR_get_physical_device_properties2)
	vkResetQueryPoolEXT = (PFN_vkResetQueryPoolEXT)vkGetInstanceProcAddr(instance,"vkResetQueryPoolEXT");
#endif

#if defined(VK_EXT_extended_dynamic_state) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)vkGetInstanceProcAddr(instance,"vkCmdSetCullModeEXT");
	vkCmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT)vkGetInstanceProcAddr(instance,"vkCmdSetFrontFaceEXT");
	vkCmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT)vkGetInstanceProcAddr(instance,"vkCmdSetPrimitiveTopologyEXT");
	vkCmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT)vkGetInstanceProcAddr(instance,"vkCmdSetViewportWithCountEXT");
	vkCmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT)vkGetInstanceProcAddr(instance,"vkCmdSetScissorWithCountEXT");
	vkCmdBindVertexBuffers2EXT = (PFN_vkCmdBindVertexBuffers2EXT)vkGetInstanceProcAddr(instance,"vkCmdBindVertexBuffers2EXT");
	vkCmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetDepthTestEnableEXT");
	vkCmdSetDepthWriteEnableEXT = (PFN_vkCmdSetDepthWriteEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetDepthWriteEnableEXT");
	vkCmdSetDepthCompareOpEXT = (PFN_vkCmdSetDepthCompareOpEXT)vkGetInstanceProcAddr(instance,"vkCmdSetDepthCompareOpEXT");
	vkCmdSetDepthBoundsTestEnableEXT = (PFN_vkCmdSetDepthBoundsTestEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetDepthBoundsTestEnableEXT");
	vkCmdSetStencilTestEnableEXT = (PFN_vkCmdSetStencilTestEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetStencilTestEnableEXT");
	vkCmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT)vkGetInstanceProcAddr(instance,"vkCmdSetStencilOpEXT");
#endif

#if defined(VK_KHR_deferred_host_operations)
	vkCreateDeferredOperationKHR = (PFN_vkCreateDeferredOperationKHR)vkGetInstanceProcAddr(instance,"vkCreateDeferredOperationKHR");
	vkDestroyDeferredOperationKHR = (PFN_vkDestroyDeferredOperationKHR)vkGetInstanceProcAddr(instance,"vkDestroyDeferredOperationKHR");
	vkGetDeferredOperationMaxConcurrencyKHR = (PFN_vkGetDeferredOperationMaxConcurrencyKHR)vkGetInstanceProcAddr(instance,"vkGetDeferredOperationMaxConcurrencyKHR");
	vkGetDeferredOperationResultKHR = (PFN_vkGetDeferredOperationResultKHR)vkGetInstanceProcAddr(instance,"vkGetDeferredOperationResultKHR");
	vkDeferredOperationJoinKHR = (PFN_vkDeferredOperationJoinKHR)vkGetInstanceProcAddr(instance,"vkDeferredOperationJoinKHR");
#endif

#if defined(VK_KHR_pipeline_executable_properties) && defined(VK_KHR_get_physical_device_properties2)
	vkGetPipelineExecutablePropertiesKHR = (PFN_vkGetPipelineExecutablePropertiesKHR)vkGetInstanceProcAddr(instance,"vkGetPipelineExecutablePropertiesKHR");
	vkGetPipelineExecutableStatisticsKHR = (PFN_vkGetPipelineExecutableStatisticsKHR)vkGetInstanceProcAddr(instance,"vkGetPipelineExecutableStatisticsKHR");
	vkGetPipelineExecutableInternalRepresentationsKHR = (PFN_vkGetPipelineExecutableInternalRepresentationsKHR)vkGetInstanceProcAddr(instance,"vkGetPipelineExecutableInternalRepresentationsKHR");
#endif

#if defined(VK_NV_device_generated_commands)
	vkGetGeneratedCommandsMemoryRequirementsNV = (PFN_vkGetGeneratedCommandsMemoryRequirementsNV)vkGetInstanceProcAddr(instance,"vkGetGeneratedCommandsMemoryRequirementsNV");
	vkCmdPreprocessGeneratedCommandsNV = (PFN_vkCmdPreprocessGeneratedCommandsNV)vkGetInstanceProcAddr(instance,"vkCmdPreprocessGeneratedCommandsNV");
	vkCmdExecuteGeneratedCommandsNV = (PFN_vkCmdExecuteGeneratedCommandsNV)vkGetInstanceProcAddr(instance,"vkCmdExecuteGeneratedCommandsNV");
	vkCmdBindPipelineShaderGroupNV = (PFN_vkCmdBindPipelineShaderGroupNV)vkGetInstanceProcAddr(instance,"vkCmdBindPipelineShaderGroupNV");
	vkCreateIndirectCommandsLayoutNV = (PFN_vkCreateIndirectCommandsLayoutNV)vkGetInstanceProcAddr(instance,"vkCreateIndirectCommandsLayoutNV");
	vkDestroyIndirectCommandsLayoutNV = (PFN_vkDestroyIndirectCommandsLayoutNV)vkGetInstanceProcAddr(instance,"vkDestroyIndirectCommandsLayoutNV");
#endif

#if defined(VK_EXT_private_data)
	vkCreatePrivateDataSlotEXT = (PFN_vkCreatePrivateDataSlotEXT)vkGetInstanceProcAddr(instance,"vkCreatePrivateDataSlotEXT");
	vkDestroyPrivateDataSlotEXT = (PFN_vkDestroyPrivateDataSlotEXT)vkGetInstanceProcAddr(instance,"vkDestroyPrivateDataSlotEXT");
	vkSetPrivateDataEXT = (PFN_vkSetPrivateDataEXT)vkGetInstanceProcAddr(instance,"vkSetPrivateDataEXT");
	vkGetPrivateDataEXT = (PFN_vkGetPrivateDataEXT)vkGetInstanceProcAddr(instance,"vkGetPrivateDataEXT");
#endif

#if defined(VK_KHR_synchronization2) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetEvent2KHR = (PFN_vkCmdSetEvent2KHR)vkGetInstanceProcAddr(instance,"vkCmdSetEvent2KHR");
	vkCmdResetEvent2KHR = (PFN_vkCmdResetEvent2KHR)vkGetInstanceProcAddr(instance,"vkCmdResetEvent2KHR");
	vkCmdWaitEvents2KHR = (PFN_vkCmdWaitEvents2KHR)vkGetInstanceProcAddr(instance,"vkCmdWaitEvents2KHR");
	vkCmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)vkGetInstanceProcAddr(instance,"vkCmdPipelineBarrier2KHR");
	vkCmdWriteTimestamp2KHR = (PFN_vkCmdWriteTimestamp2KHR)vkGetInstanceProcAddr(instance,"vkCmdWriteTimestamp2KHR");
	vkQueueSubmit2KHR = (PFN_vkQueueSubmit2KHR)vkGetInstanceProcAddr(instance,"vkQueueSubmit2KHR");
#if defined(VK_AMD_buffer_marker)
	vkCmdWriteBufferMarker2AMD = (PFN_vkCmdWriteBufferMarker2AMD)vkGetInstanceProcAddr(instance,"vkCmdWriteBufferMarker2AMD");
#endif
#if defined(VK_NV_device_diagnostic_checkpoints)
	vkGetQueueCheckpointData2NV = (PFN_vkGetQueueCheckpointData2NV)vkGetInstanceProcAddr(instance,"vkGetQueueCheckpointData2NV");
#endif
#endif

#if defined(VK_NV_fragment_shading_rate_enums) && defined(VK_KHR_fragment_shading_rate)
	vkCmdSetFragmentShadingRateEnumNV = (PFN_vkCmdSetFragmentShadingRateEnumNV)vkGetInstanceProcAddr(instance,"vkCmdSetFragmentShadingRateEnumNV");
#endif

#if defined(VK_KHR_copy_commands2)
	vkCmdCopyBuffer2KHR = (PFN_vkCmdCopyBuffer2KHR)vkGetInstanceProcAddr(instance,"vkCmdCopyBuffer2KHR");
	vkCmdCopyImage2KHR = (PFN_vkCmdCopyImage2KHR)vkGetInstanceProcAddr(instance,"vkCmdCopyImage2KHR");
	vkCmdCopyBufferToImage2KHR = (PFN_vkCmdCopyBufferToImage2KHR)vkGetInstanceProcAddr(instance,"vkCmdCopyBufferToImage2KHR");
	vkCmdCopyImageToBuffer2KHR = (PFN_vkCmdCopyImageToBuffer2KHR)vkGetInstanceProcAddr(instance,"vkCmdCopyImageToBuffer2KHR");
	vkCmdBlitImage2KHR = (PFN_vkCmdBlitImage2KHR)vkGetInstanceProcAddr(instance,"vkCmdBlitImage2KHR");
	vkCmdResolveImage2KHR = (PFN_vkCmdResolveImage2KHR)vkGetInstanceProcAddr(instance,"vkCmdResolveImage2KHR");
#endif

#if defined(VK_NV_acquire_winrt_display) && defined(VK_EXT_direct_mode_display)
	vkAcquireWinrtDisplayNV = (PFN_vkAcquireWinrtDisplayNV)vkGetInstanceProcAddr(instance,"vkAcquireWinrtDisplayNV");
	vkGetWinrtDisplayNV = (PFN_vkGetWinrtDisplayNV)vkGetInstanceProcAddr(instance,"vkGetWinrtDisplayNV");
#endif

#if defined(VK_EXT_directfb_surface) && defined(VK_KHR_surface)
	vkCreateDirectFBSurfaceEXT = (PFN_vkCreateDirectFBSurfaceEXT)vkGetInstanceProcAddr(instance,"vkCreateDirectFBSurfaceEXT");
	vkGetPhysicalDeviceDirectFBPresentationSupportEXT = (PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceDirectFBPresentationSupportEXT");
#endif

#if defined(VK_EXT_vertex_input_dynamic_state) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)vkGetInstanceProcAddr(instance,"vkCmdSetVertexInputEXT");
#endif

#if defined(VK_FUCHSIA_external_memory) && defined(VK_KHR_external_memory_capabilities) && defined(VK_KHR_external_memory)
	vkGetMemoryZirconHandleFUCHSIA = (PFN_vkGetMemoryZirconHandleFUCHSIA)vkGetInstanceProcAddr(instance,"vkGetMemoryZirconHandleFUCHSIA");
	vkGetMemoryZirconHandlePropertiesFUCHSIA = (PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA)vkGetInstanceProcAddr(instance,"vkGetMemoryZirconHandlePropertiesFUCHSIA");
#endif

#if defined(VK_FUCHSIA_external_semaphore) && defined(VK_KHR_external_semaphore_capabilities) && defined(VK_KHR_external_semaphore)
	vkImportSemaphoreZirconHandleFUCHSIA = (PFN_vkImportSemaphoreZirconHandleFUCHSIA)vkGetInstanceProcAddr(instance,"vkImportSemaphoreZirconHandleFUCHSIA");
	vkGetSemaphoreZirconHandleFUCHSIA = (PFN_vkGetSemaphoreZirconHandleFUCHSIA)vkGetInstanceProcAddr(instance,"vkGetSemaphoreZirconHandleFUCHSIA");
#endif

#if defined(VK_EXT_extended_dynamic_state2) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetPatchControlPointsEXT = (PFN_vkCmdSetPatchControlPointsEXT)vkGetInstanceProcAddr(instance,"vkCmdSetPatchControlPointsEXT");
	vkCmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetRasterizerDiscardEnableEXT");
	vkCmdSetDepthBiasEnableEXT = (PFN_vkCmdSetDepthBiasEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetDepthBiasEnableEXT");
	vkCmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT)vkGetInstanceProcAddr(instance,"vkCmdSetLogicOpEXT");
	vkCmdSetPrimitiveRestartEnableEXT = (PFN_vkCmdSetPrimitiveRestartEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetPrimitiveRestartEnableEXT");
#endif

#if defined(VK_QNX_screen_surface) && defined(VK_KHR_surface)
	vkCreateScreenSurfaceQNX = (PFN_vkCreateScreenSurfaceQNX)vkGetInstanceProcAddr(instance,"vkCreateScreenSurfaceQNX");
	vkGetPhysicalDeviceScreenPresentationSupportQNX = (PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceScreenPresentationSupportQNX");
#endif

#if defined(VK_EXT_color_write_enable) && defined(VK_KHR_get_physical_device_properties2)
	vkCmdSetColorWriteEnableEXT = (PFN_vkCmdSetColorWriteEnableEXT)vkGetInstanceProcAddr(instance,"vkCmdSetColorWriteEnableEXT");
#endif


  // End of generated code
}

// Generated code example: "PFN_vkCreateInstance vkCreateInstance"
// GENERATED VULKAN API SYMBOLS DECLARATION
#ifdef VK_VERSION_1_0
PFN_vkCreateInstance vkCreateInstance;
PFN_vkDestroyInstance vkDestroyInstance;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
PFN_vkCreateDevice vkCreateDevice;
PFN_vkDestroyDevice vkDestroyDevice;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
PFN_vkGetDeviceQueue vkGetDeviceQueue;
PFN_vkQueueSubmit vkQueueSubmit;
PFN_vkQueueWaitIdle vkQueueWaitIdle;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
PFN_vkAllocateMemory vkAllocateMemory;
PFN_vkFreeMemory vkFreeMemory;
PFN_vkMapMemory vkMapMemory;
PFN_vkUnmapMemory vkUnmapMemory;
PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
PFN_vkBindBufferMemory vkBindBufferMemory;
PFN_vkBindImageMemory vkBindImageMemory;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
PFN_vkQueueBindSparse vkQueueBindSparse;
PFN_vkCreateFence vkCreateFence;
PFN_vkDestroyFence vkDestroyFence;
PFN_vkResetFences vkResetFences;
PFN_vkGetFenceStatus vkGetFenceStatus;
PFN_vkWaitForFences vkWaitForFences;
PFN_vkCreateSemaphore vkCreateSemaphore;
PFN_vkDestroySemaphore vkDestroySemaphore;
PFN_vkCreateEvent vkCreateEvent;
PFN_vkDestroyEvent vkDestroyEvent;
PFN_vkGetEventStatus vkGetEventStatus;
PFN_vkSetEvent vkSetEvent;
PFN_vkResetEvent vkResetEvent;
PFN_vkCreateQueryPool vkCreateQueryPool;
PFN_vkDestroyQueryPool vkDestroyQueryPool;
PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
PFN_vkCreateBuffer vkCreateBuffer;
PFN_vkDestroyBuffer vkDestroyBuffer;
PFN_vkCreateBufferView vkCreateBufferView;
PFN_vkDestroyBufferView vkDestroyBufferView;
PFN_vkCreateImage vkCreateImage;
PFN_vkDestroyImage vkDestroyImage;
PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
PFN_vkCreateImageView vkCreateImageView;
PFN_vkDestroyImageView vkDestroyImageView;
PFN_vkCreateShaderModule vkCreateShaderModule;
PFN_vkDestroyShaderModule vkDestroyShaderModule;
PFN_vkCreatePipelineCache vkCreatePipelineCache;
PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
PFN_vkMergePipelineCaches vkMergePipelineCaches;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines vkCreateComputePipelines;
PFN_vkDestroyPipeline vkDestroyPipeline;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
PFN_vkCreateSampler vkCreateSampler;
PFN_vkDestroySampler vkDestroySampler;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
PFN_vkResetDescriptorPool vkResetDescriptorPool;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
PFN_vkCreateFramebuffer vkCreateFramebuffer;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
PFN_vkCreateRenderPass vkCreateRenderPass;
PFN_vkDestroyRenderPass vkDestroyRenderPass;
PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
PFN_vkCreateCommandPool vkCreateCommandPool;
PFN_vkDestroyCommandPool vkDestroyCommandPool;
PFN_vkResetCommandPool vkResetCommandPool;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
PFN_vkEndCommandBuffer vkEndCommandBuffer;
PFN_vkResetCommandBuffer vkResetCommandBuffer;
PFN_vkCmdBindPipeline vkCmdBindPipeline;
PFN_vkCmdSetViewport vkCmdSetViewport;
PFN_vkCmdSetScissor vkCmdSetScissor;
PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
PFN_vkCmdDraw vkCmdDraw;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
PFN_vkCmdDispatch vkCmdDispatch;
PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
PFN_vkCmdCopyImage vkCmdCopyImage;
PFN_vkCmdBlitImage vkCmdBlitImage;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
PFN_vkCmdFillBuffer vkCmdFillBuffer;
PFN_vkCmdClearColorImage vkCmdClearColorImage;
PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
PFN_vkCmdClearAttachments vkCmdClearAttachments;
PFN_vkCmdResolveImage vkCmdResolveImage;
PFN_vkCmdSetEvent vkCmdSetEvent;
PFN_vkCmdResetEvent vkCmdResetEvent;
PFN_vkCmdWaitEvents vkCmdWaitEvents;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
PFN_vkCmdBeginQuery vkCmdBeginQuery;
PFN_vkCmdEndQuery vkCmdEndQuery;
PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
PFN_vkCmdPushConstants vkCmdPushConstants;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
PFN_vkCmdNextSubpass vkCmdNextSubpass;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
#endif

#ifdef VK_VERSION_1_1
PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
PFN_vkBindBufferMemory2 vkBindBufferMemory2;
PFN_vkBindImageMemory2 vkBindImageMemory2;
PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
PFN_vkCmdDispatchBase vkCmdDispatchBase;
PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2;
PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkGetPhysicalDeviceQueueFamilyProperties2;
PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 vkGetPhysicalDeviceSparseImageFormatProperties2;
PFN_vkTrimCommandPool vkTrimCommandPool;
PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
PFN_vkGetPhysicalDeviceExternalBufferProperties vkGetPhysicalDeviceExternalBufferProperties;
PFN_vkGetPhysicalDeviceExternalFenceProperties vkGetPhysicalDeviceExternalFenceProperties;
PFN_vkGetPhysicalDeviceExternalSemaphoreProperties vkGetPhysicalDeviceExternalSemaphoreProperties;
PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
#endif

#ifdef VK_VERSION_1_2
PFN_vkCmdDrawIndirectCount vkCmdDrawIndirectCount;
PFN_vkCmdDrawIndexedIndirectCount vkCmdDrawIndexedIndirectCount;
PFN_vkCreateRenderPass2 vkCreateRenderPass2;
PFN_vkCmdBeginRenderPass2 vkCmdBeginRenderPass2;
PFN_vkCmdNextSubpass2 vkCmdNextSubpass2;
PFN_vkCmdEndRenderPass2 vkCmdEndRenderPass2;
PFN_vkResetQueryPool vkResetQueryPool;
PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue;
PFN_vkWaitSemaphores vkWaitSemaphores;
PFN_vkSignalSemaphore vkSignalSemaphore;
PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
PFN_vkGetBufferOpaqueCaptureAddress vkGetBufferOpaqueCaptureAddress;
PFN_vkGetDeviceMemoryOpaqueCaptureAddress vkGetDeviceMemoryOpaqueCaptureAddress;
#endif

#if defined(VK_KHR_surface)
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
#endif

#if defined(VK_KHR_swapchain) && defined(VK_KHR_surface)
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
PFN_vkQueuePresentKHR vkQueuePresentKHR;
#if defined(VK_VERSION_1_1)
PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif
#endif

#if defined(VK_KHR_display) && defined(VK_KHR_surface)
PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
#endif

#if defined(VK_KHR_display_swapchain) && defined(VK_KHR_swapchain) && defined(VK_KHR_display)
PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#endif

#if defined(VK_KHR_xlib_surface) && defined(VK_KHR_surface)
PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif

#if defined(VK_KHR_xcb_surface) && defined(VK_KHR_surface)
PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif

#if defined(VK_KHR_wayland_surface) && defined(VK_KHR_surface)
PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif

#if defined(VK_KHR_android_surface) && defined(VK_KHR_surface)
PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif

#if defined(VK_KHR_win32_surface) && defined(VK_KHR_surface)
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif

#if defined(VK_EXT_debug_report)
PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
#endif

#if defined(VK_EXT_debug_marker) && defined(VK_EXT_debug_report)
PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
#endif

#if defined(VK_KHR_video_queue) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_sampler_ycbcr_conversion)
PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR;
PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR;
PFN_vkCreateVideoSessionKHR vkCreateVideoSessionKHR;
PFN_vkDestroyVideoSessionKHR vkDestroyVideoSessionKHR;
PFN_vkGetVideoSessionMemoryRequirementsKHR vkGetVideoSessionMemoryRequirementsKHR;
PFN_vkBindVideoSessionMemoryKHR vkBindVideoSessionMemoryKHR;
PFN_vkCreateVideoSessionParametersKHR vkCreateVideoSessionParametersKHR;
PFN_vkUpdateVideoSessionParametersKHR vkUpdateVideoSessionParametersKHR;
PFN_vkDestroyVideoSessionParametersKHR vkDestroyVideoSessionParametersKHR;
PFN_vkCmdBeginVideoCodingKHR vkCmdBeginVideoCodingKHR;
PFN_vkCmdEndVideoCodingKHR vkCmdEndVideoCodingKHR;
PFN_vkCmdControlVideoCodingKHR vkCmdControlVideoCodingKHR;
#endif

#if defined(VK_KHR_video_decode_queue) && defined(VK_KHR_video_queue) && defined(VK_KHR_synchronization2)
PFN_vkCmdDecodeVideoKHR vkCmdDecodeVideoKHR;
#endif

#if defined(VK_KHR_video_encode_queue) && defined(VK_KHR_video_queue) && defined(VK_KHR_synchronization2)
PFN_vkCmdEncodeVideoKHR vkCmdEncodeVideoKHR;
#endif

#if defined(VK_EXT_transform_feedback) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
#endif

#if defined(VK_NVX_image_view_handle)
PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
#endif

#if defined(VK_AMD_draw_indirect_count)
PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
#endif

#if defined(VK_AMD_shader_info)
PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#endif

#if defined(VK_GGP_stream_descriptor_surface) && defined(VK_KHR_surface)
PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#endif

#if defined(VK_NV_external_memory_capabilities)
PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif

#if defined(VK_NV_external_memory_win32) && defined(VK_NV_external_memory)
PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif

#if defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif

#if defined(VK_KHR_device_group) && defined(VK_KHR_device_group_creation)
PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
#if defined(VK_KHR_surface)
PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
#endif
#if defined(VK_KHR_swapchain)
PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif
#endif

#if defined(VK_NN_vi_surface) && defined(VK_KHR_surface)
PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#endif

#if defined(VK_KHR_maintenance1)
PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#endif

#if defined(VK_KHR_device_group_creation)
PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
#endif

#if defined(VK_KHR_external_memory_capabilities) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
#endif

#if defined(VK_KHR_external_memory_win32) && defined(VK_KHR_external_memory)
PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif

#if defined(VK_KHR_external_memory_fd) && defined(VK_KHR_external_memory)
PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_capabilities) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_win32) && defined(VK_KHR_external_semaphore)
PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
#endif

#if defined(VK_KHR_external_semaphore_fd) && defined(VK_KHR_external_semaphore)
PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
#endif

#if defined(VK_KHR_push_descriptor) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#if defined(VK_VERSION_1_1)
PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif
#if defined(VK_KHR_descriptor_update_template)
PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif
#endif

#if defined(VK_EXT_conditional_rendering)
PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#endif

#if defined(VK_KHR_descriptor_update_template)
PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#if defined(VK_KHR_push_descriptor)
PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif
#endif

#if defined(VK_NV_clip_space_w_scaling)
PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#endif

#if defined(VK_EXT_direct_mode_display) && defined(VK_KHR_display)
PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
#endif

#if defined(VK_EXT_acquire_xlib_display) && defined(VK_EXT_direct_mode_display)
PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#endif

#if defined(VK_EXT_display_surface_counter) && defined(VK_KHR_display)
PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
#endif

#if defined(VK_EXT_display_control) && defined(VK_EXT_display_surface_counter) && defined(VK_KHR_swapchain)
PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
#endif

#if defined(VK_GOOGLE_display_timing) && defined(VK_KHR_swapchain)
PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
#endif

#if defined(VK_EXT_discard_rectangles) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#endif

#if defined(VK_EXT_hdr_metadata) && defined(VK_KHR_swapchain)
PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#endif

#if defined(VK_KHR_create_renderpass2) && defined(VK_KHR_multiview) && defined(VK_KHR_maintenance2)
PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
#endif

#if defined(VK_KHR_shared_presentable_image) && defined(VK_KHR_swapchain) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_get_surface_capabilities2)
PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#endif

#if defined(VK_KHR_external_fence_capabilities) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
#endif

#if defined(VK_KHR_external_fence_win32) && defined(VK_KHR_external_fence)
PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
#endif

#if defined(VK_KHR_external_fence_fd) && defined(VK_KHR_external_fence)
PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
#endif

#if defined(VK_KHR_performance_query) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#endif

#if defined(VK_KHR_get_surface_capabilities2) && defined(VK_KHR_surface)
PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
#endif

#if defined(VK_KHR_get_display_properties2) && defined(VK_KHR_display)
PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
#endif

#if defined(VK_MVK_ios_surface) && defined(VK_KHR_surface)
PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#endif

#if defined(VK_MVK_macos_surface) && defined(VK_KHR_surface)
PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif

#if defined(VK_EXT_debug_utils)
PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT;
PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;
PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
#endif

#if defined(VK_ANDROID_external_memory_android_hardware_buffer) && defined(VK_KHR_sampler_ycbcr_conversion) && defined(VK_KHR_external_memory) && defined(VK_EXT_queue_family_foreign) && defined(VK_KHR_dedicated_allocation)
PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif

#if defined(VK_EXT_sample_locations) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif

#if defined(VK_KHR_get_memory_requirements2)
PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#endif

#if defined(VK_KHR_acceleration_structure) && defined(VK_EXT_descriptor_indexing) && defined(VK_KHR_buffer_device_address) && defined(VK_KHR_deferred_host_operations)
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
PFN_vkCmdBuildAccelerationStructuresIndirectKHR vkCmdBuildAccelerationStructuresIndirectKHR;
PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
PFN_vkCopyAccelerationStructureKHR vkCopyAccelerationStructureKHR;
PFN_vkCopyAccelerationStructureToMemoryKHR vkCopyAccelerationStructureToMemoryKHR;
PFN_vkCopyMemoryToAccelerationStructureKHR vkCopyMemoryToAccelerationStructureKHR;
PFN_vkWriteAccelerationStructuresPropertiesKHR vkWriteAccelerationStructuresPropertiesKHR;
PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
PFN_vkCmdCopyAccelerationStructureToMemoryKHR vkCmdCopyAccelerationStructureToMemoryKHR;
PFN_vkCmdCopyMemoryToAccelerationStructureKHR vkCmdCopyMemoryToAccelerationStructureKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
PFN_vkGetDeviceAccelerationStructureCompatibilityKHR vkGetDeviceAccelerationStructureCompatibilityKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
#endif

#if defined(VK_KHR_ray_tracing_pipeline) && defined(VK_KHR_spirv_1_4) && defined(VK_KHR_acceleration_structure)
PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR vkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
PFN_vkCmdTraceRaysIndirectKHR vkCmdTraceRaysIndirectKHR;
PFN_vkGetRayTracingShaderGroupStackSizeKHR vkGetRayTracingShaderGroupStackSizeKHR;
PFN_vkCmdSetRayTracingPipelineStackSizeKHR vkCmdSetRayTracingPipelineStackSizeKHR;
#endif

#if defined(VK_KHR_sampler_ycbcr_conversion) && defined(VK_KHR_maintenance1) && defined(VK_KHR_bind_memory2) && defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#endif

#if defined(VK_KHR_bind_memory2)
PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#endif

#if defined(VK_EXT_image_drm_format_modifier) && defined(VK_KHR_bind_memory2) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_image_format_list) && defined(VK_KHR_sampler_ycbcr_conversion)
PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#endif

#if defined(VK_EXT_validation_cache)
PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
#endif

#if defined(VK_NV_shading_rate_image) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
#endif

#if defined(VK_NV_ray_tracing) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_get_memory_requirements2)
PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
PFN_vkCompileDeferredNV vkCompileDeferredNV;
#endif

#if defined(VK_KHR_maintenance3) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#endif

#if defined(VK_KHR_draw_indirect_count)
PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
#endif

#if defined(VK_EXT_external_memory_host) && defined(VK_KHR_external_memory)
PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#endif

#if defined(VK_AMD_buffer_marker)
PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#endif

#if defined(VK_EXT_calibrated_timestamps)
PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
#endif

#if defined(VK_NV_mesh_shader) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
#endif

#if defined(VK_NV_scissor_exclusive) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#endif

#if defined(VK_NV_device_diagnostic_checkpoints) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#endif

#if defined(VK_KHR_timeline_semaphore) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
#endif

#if defined(VK_INTEL_performance_query)
PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
#endif

#if defined(VK_AMD_display_native_hdr) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_get_surface_capabilities2) && defined(VK_KHR_swapchain)
PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#endif

#if defined(VK_FUCHSIA_imagepipe_surface) && defined(VK_KHR_surface)
PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#endif

#if defined(VK_EXT_metal_surface) && defined(VK_KHR_surface)
PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#endif

#if defined(VK_KHR_fragment_shading_rate) && defined(VK_KHR_create_renderpass2) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;
#endif

#if defined(VK_EXT_buffer_device_address) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#endif

#if defined(VK_EXT_tooling_info)
PFN_vkGetPhysicalDeviceToolPropertiesEXT vkGetPhysicalDeviceToolPropertiesEXT;
#endif

#if defined(VK_NV_cooperative_matrix) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif

#if defined(VK_NV_coverage_reduction_mode) && defined(VK_NV_framebuffer_mixed_samples)
PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#endif

#if defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_get_physical_device_properties2) && defined(VK_KHR_surface) && defined(VK_KHR_get_surface_capabilities2) && defined(VK_KHR_swapchain)
PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#if defined(VK_KHR_device_group)
PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif
#if defined(VK_VERSION_1_1)
PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif
#endif

#if defined(VK_EXT_headless_surface) && defined(VK_KHR_surface)
PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
#endif

#if defined(VK_KHR_buffer_device_address) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#endif

#if defined(VK_EXT_line_rasterization) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#endif

#if defined(VK_EXT_host_query_reset) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#endif

#if defined(VK_EXT_extended_dynamic_state) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;
PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT;
PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT;
PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT;
PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT;
PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT;
PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
#endif

#if defined(VK_KHR_deferred_host_operations)
PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
PFN_vkGetDeferredOperationMaxConcurrencyKHR vkGetDeferredOperationMaxConcurrencyKHR;
PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
#endif

#if defined(VK_KHR_pipeline_executable_properties) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
PFN_vkGetPipelineExecutableInternalRepresentationsKHR vkGetPipelineExecutableInternalRepresentationsKHR;
#endif

#if defined(VK_NV_device_generated_commands)
PFN_vkGetGeneratedCommandsMemoryRequirementsNV vkGetGeneratedCommandsMemoryRequirementsNV;
PFN_vkCmdPreprocessGeneratedCommandsNV vkCmdPreprocessGeneratedCommandsNV;
PFN_vkCmdExecuteGeneratedCommandsNV vkCmdExecuteGeneratedCommandsNV;
PFN_vkCmdBindPipelineShaderGroupNV vkCmdBindPipelineShaderGroupNV;
PFN_vkCreateIndirectCommandsLayoutNV vkCreateIndirectCommandsLayoutNV;
PFN_vkDestroyIndirectCommandsLayoutNV vkDestroyIndirectCommandsLayoutNV;
#endif

#if defined(VK_EXT_private_data)
PFN_vkCreatePrivateDataSlotEXT vkCreatePrivateDataSlotEXT;
PFN_vkDestroyPrivateDataSlotEXT vkDestroyPrivateDataSlotEXT;
PFN_vkSetPrivateDataEXT vkSetPrivateDataEXT;
PFN_vkGetPrivateDataEXT vkGetPrivateDataEXT;
#endif

#if defined(VK_KHR_synchronization2) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR;
PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR;
PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR;
PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;
PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR;
PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
#if defined(VK_AMD_buffer_marker)
PFN_vkCmdWriteBufferMarker2AMD vkCmdWriteBufferMarker2AMD;
#endif
#if defined(VK_NV_device_diagnostic_checkpoints)
PFN_vkGetQueueCheckpointData2NV vkGetQueueCheckpointData2NV;
#endif
#endif

#if defined(VK_NV_fragment_shading_rate_enums) && defined(VK_KHR_fragment_shading_rate)
PFN_vkCmdSetFragmentShadingRateEnumNV vkCmdSetFragmentShadingRateEnumNV;
#endif

#if defined(VK_KHR_copy_commands2)
PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR;
PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR;
PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR;
PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR;
PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR;
PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR;
#endif

#if defined(VK_NV_acquire_winrt_display) && defined(VK_EXT_direct_mode_display)
PFN_vkAcquireWinrtDisplayNV vkAcquireWinrtDisplayNV;
PFN_vkGetWinrtDisplayNV vkGetWinrtDisplayNV;
#endif

#if defined(VK_EXT_directfb_surface) && defined(VK_KHR_surface)
PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT;
PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT vkGetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif

#if defined(VK_EXT_vertex_input_dynamic_state) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
#endif

#if defined(VK_FUCHSIA_external_memory) && defined(VK_KHR_external_memory_capabilities) && defined(VK_KHR_external_memory)
PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA;
PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA vkGetMemoryZirconHandlePropertiesFUCHSIA;
#endif

#if defined(VK_FUCHSIA_external_semaphore) && defined(VK_KHR_external_semaphore_capabilities) && defined(VK_KHR_external_semaphore)
PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA;
PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA;
#endif

#if defined(VK_EXT_extended_dynamic_state2) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;
PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
#endif

#if defined(VK_QNX_screen_surface) && defined(VK_KHR_surface)
PFN_vkCreateScreenSurfaceQNX vkCreateScreenSurfaceQNX;
PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX vkGetPhysicalDeviceScreenPresentationSupportQNX;
#endif

#if defined(VK_EXT_color_write_enable) && defined(VK_KHR_get_physical_device_properties2)
PFN_vkCmdSetColorWriteEnableEXT vkCmdSetColorWriteEnableEXT;
#endif


// End of generated code

#endif