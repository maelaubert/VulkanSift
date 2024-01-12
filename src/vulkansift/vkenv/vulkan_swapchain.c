#include "vulkan_swapchain.h"
#include "logger.h"
#include "vulkan_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char LOG_TAG[] = "VulkanSwapchain";

bool checkDeviceSwapchainSupport(vkenv_Device device, VkSurfaceKHR surface);
bool createSwapchain(vkenv_Device device, vkenv_Swapchain swapchain, VkSurfaceKHR surface, vkenv_SwapchainPreferences *swapchain_pref);

bool vkenv_createSwapchain(vkenv_Device device, vkenv_Swapchain *swapchain_ptr, VkSurfaceKHR surface, vkenv_SwapchainPreferences *swapchain_pref)
{
  assert(device != NULL);
  assert(swapchain_ptr != NULL);
  assert(swapchain_pref != NULL);

  // Allocate vkenv_Swapchain instance
  *swapchain_ptr = (vkenv_Swapchain)malloc(sizeof(struct vkenv_Swapchain_T));
  vkenv_Swapchain swapchain = *swapchain_ptr;
  // Zeroing structure for safe creation and destruction
  memset(swapchain, 0, sizeof(struct vkenv_Swapchain_T));

  if (checkDeviceSwapchainSupport(device, surface) && createSwapchain(device, swapchain, surface, swapchain_pref))
  {
    return true;
  }
  else
  {
    logError(LOG_TAG, "vkenv_Swapchain creation failed.");
    vkenv_destroySwapchain(device, swapchain_ptr);
    return false;
  }
}

void vkenv_destroySwapchain(vkenv_Device device, vkenv_Swapchain *swapchain_ptr)
{
  assert(device != NULL);
  assert(swapchain_ptr != NULL);
  assert(*swapchain_ptr != NULL); // destroySwapchain shouldn't be called on NULL Swapchain

  vkenv_Swapchain swapchain = *swapchain_ptr;

  // Destroy image views
  for (uint32_t i = 0; i < swapchain->nb_swapchain_image; i++)
  {
    VK_NULL_SAFE_DELETE(swapchain->swapchain_image_views[i], vkDestroyImageView(device->device, swapchain->swapchain_image_views[i], NULL));
  }
  free(swapchain->swapchain_image_views);

  // Destroy swapchain (automatically destroy swapchain images)
  VK_NULL_SAFE_DELETE(swapchain->swapchain, vkDestroySwapchainKHR(device->device, swapchain->swapchain, NULL));
  free(swapchain->swapchain_images);

  // Delete vkenv_Swapchain instance
  free(*swapchain_ptr);
  *swapchain_ptr = NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool checkDeviceSwapchainSupport(vkenv_Device device, VkSurfaceKHR surface)
{
  VkBool32 surface_support = 0;
  vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device, device->general_queues_family_idx, surface, &surface_support);
  if (surface_support == 0)
  {
    logError(LOG_TAG, "Physical device doesn't support acquired surface");
    return false;
  }
  // Check that the device can find the swapchain related function addresses. This only possible if the device supports the
  // VK_KHR_SWAPCHAIN_EXTENSION_NAME extension, that is needed to setup the swapchain.
  if (vkGetDeviceProcAddr(device->device, "vkCreateSwapchainKHR") == NULL || vkGetDeviceProcAddr(device->device, "vkGetSwapchainImagesKHR") == NULL ||
      vkGetDeviceProcAddr(device->device, "vkAcquireNextImageKHR") == NULL || vkGetDeviceProcAddr(device->device, "vkQueuePresentKHR") == NULL ||
      vkGetDeviceProcAddr(device->device, "vkDestroySwapchainKHR") == NULL)
  {
    logError(LOG_TAG, "Failed to access the device functions associated to the VK_KHR_swapchain extension. This extension is needed to setup and use a "
                      "swapchain, please make sure that the vkenv_Device enables this extension.");
    return false;
  }
  return true;
}

bool createSwapchain(vkenv_Device device, vkenv_Swapchain swapchain, VkSurfaceKHR surface, vkenv_SwapchainPreferences *swapchain_pref)
{
  // Select the swapchain format
  uint32_t surface_format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, surface, &surface_format_count, NULL);
  if (surface_format_count == 0)
  {
    logError(LOG_TAG, "No SurfaceFormat supported by the acquired surface. Impossible to create a valid swapchain.");
    return false;
  }
  VkSurfaceFormatKHR *surface_formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * surface_format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, surface, &surface_format_count, surface_formats);
  // Try to find the preferred format in all the available formats. If not found use VK_FORMAT_B8G8R8A8_UNORM as a fallback solution.
  VkSurfaceFormatKHR selected_swapchain_format = {.format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  for (uint32_t i = 0; i < surface_format_count; i++)
  {
    if (surface_formats[i].format == swapchain_pref->format)
    {
      selected_swapchain_format = surface_formats[i];
      break;
    }
  }
  free(surface_formats);

  // Try to find the preferred present mode in all the available modes. If not found use VK_PRESENT_MODE_FIFO_KHR as a fallback solution.
  uint32_t presentation_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, surface, &presentation_mode_count, NULL);
  if (presentation_mode_count == 0)
  {
    logError(LOG_TAG, "No PresnetMode supported by the acquired surface. Impossible to create a valid swapchain.");
    return false;
  }
  VkPresentModeKHR *presentation_modes = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * presentation_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, surface, &presentation_mode_count, presentation_modes);
  // Always select the first format by default, if found in the available formats, we can switch to a format supporting VK_FORMAT_B8G8R8A8_UNORM
  // (always supported by graphics enabled GPUs)
  VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (uint32_t i = 0; i < presentation_mode_count; i++)
  {
    if (presentation_modes[i] == swapchain_pref->present_mode)
    {
      selected_present_mode = presentation_modes[i];
      break;
    }
  }
  free(presentation_modes);

  // Retrieve surface capabilities and define swapchain extent and number of images
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, surface, &surface_capabilities);
  // Define the number of images
  uint32_t selected_swapchain_image_count = surface_capabilities.minImageCount + 1;
  if (selected_swapchain_image_count > surface_capabilities.maxImageCount)
  {
    selected_swapchain_image_count = surface_capabilities.maxImageCount;
  }

  // Select swapchain extent
  VkExtent2D selected_swapchain_extent;
  if (surface_capabilities.currentExtent.width == UINT32_MAX || surface_capabilities.currentExtent.height == UINT32_MAX)
  {
    // Special case, the surface size will be determined by the swapchain configuration (maybe only happening with Wayland)
    selected_swapchain_extent.width = swapchain_pref->width;
    selected_swapchain_extent.height = swapchain_pref->height;
  }
  else
  {
    // Use the current surface extent
    selected_swapchain_extent = surface_capabilities.currentExtent;
  }

  // Pre-transform is set to the current transform. If rotated by 90 or 270 degrees, swap the extent widht and height
  VkSurfaceTransformFlagsKHR selected_swapchain_pretransform = surface_capabilities.currentTransform;
  if (selected_swapchain_pretransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
      selected_swapchain_pretransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
  {
    uint32_t tmp = selected_swapchain_extent.width;
    selected_swapchain_extent.width = selected_swapchain_extent.height;
    selected_swapchain_extent.height = tmp;
  }

  // Create swapchain
  VkSwapchainCreateInfoKHR swapchain_create_info = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                    .pNext = NULL,
                                                    .flags = 0,
                                                    .surface = surface,
                                                    .minImageCount = selected_swapchain_image_count,
                                                    .imageFormat = selected_swapchain_format.format,
                                                    .imageColorSpace = selected_swapchain_format.colorSpace,
                                                    .imageExtent = selected_swapchain_extent,
                                                    .imageArrayLayers = 1,
                                                    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                    .queueFamilyIndexCount = 0,
                                                    .pQueueFamilyIndices = NULL,
                                                    .preTransform = selected_swapchain_pretransform,
                                                    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                    .presentMode = selected_present_mode,
                                                    .clipped = VK_TRUE,
                                                    .oldSwapchain = VK_NULL_HANDLE};

  VkResult vkres = vkCreateSwapchainKHR(device->device, &swapchain_create_info, NULL, &swapchain->swapchain);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "vkCreateSwapchainKHR failure: %s", vkenv_getVkResultString(vkres));
    return false;
  }

  // Store useful swapchain information
  swapchain->format = selected_swapchain_format.format;
  swapchain->extent = selected_swapchain_extent;
  swapchain->colorspace = selected_swapchain_format.colorSpace;
  swapchain->present_mode = selected_present_mode;
  swapchain->transform = selected_swapchain_pretransform;

  // Retrieve the number of swapchain image (might be different than the one provided in vkCreateSwapchainKHR)
  vkres = vkGetSwapchainImagesKHR(device->device, swapchain->swapchain, &swapchain->nb_swapchain_image, NULL);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failure to retrieving the number of swapchain image (vkGetSwapchainImagesKHR: %s)", vkenv_getVkResultString(vkres));
    return false;
  }

  // Allocate space to store swapchain images
  swapchain->swapchain_images = (VkImage *)malloc(sizeof(VkImage) * swapchain->nb_swapchain_image);
  vkres = vkGetSwapchainImagesKHR(device->device, swapchain->swapchain, &swapchain->nb_swapchain_image, swapchain->swapchain_images);
  if (vkres != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to retrieve the swapchain images (vkGetSwapchainImagesKHR: %s)", vkenv_getVkResultString(vkres));
    return false;
  }

  // Create swapchain image views
  swapchain->swapchain_image_views = (VkImageView *)malloc(sizeof(VkImageView) * swapchain->nb_swapchain_image);
  for (size_t i = 0; i < swapchain->nb_swapchain_image; i++)
  {
    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = swapchain->swapchain_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain->format,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

    vkres = vkCreateImageView(device->device, &image_view_create_info, NULL, &swapchain->swapchain_image_views[i]);
    if (vkres != VK_SUCCESS)
    {
      logError(LOG_TAG, "Failed to create swapchain image views (vkCreateImageView: %s", vkenv_getVkResultString(vkres));
      return false;
    }
  }

  return true;
}