#ifndef VKENV_SWAPCHAIN_H
#define VKENV_SWAPCHAIN_H

#include "vulkan_device.h"

typedef struct vkenv_Swapchain_T
{
  VkSwapchainKHR swapchain;
  // Swapchain resources
  uint32_t nb_swapchain_image;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  // Swapchain info
  VkFormat format;
  VkColorSpaceKHR colorspace;
  VkSurfaceTransformFlagsKHR transform;
  VkPresentModeKHR present_mode;
  VkExtent2D extent;
} * vkenv_Swapchain;

typedef struct
{
  uint32_t width;
  uint32_t height;
  VkFormat format;
  VkPresentModeKHR present_mode;
} vkenv_SwapchainPreferences;

// Allocate and setup a Vulkan swapchain according to the provided preferences.
// When setting up the Swapchain, the preferred surface with and height are only used if the surface does not provide its own geometry.
// The preferred format is used if available, otherwise we fallback to VK_FORMAT_B8G8R8A8_UNORM (widely available).
// The preferred present_mode is used if available, otherwise we fallback to VK_PRESENT_MODE_FIFO_KHR (widely available).
// The surface pre-transform is always defined by the current transform.
//
// Android considerations: if the transform is rotated by 90 or 270 degrees the extent width and height are swapped and the swapchain user must adapt
// any MVP matrix to perform the same rotation (https://android-developers.googleblog.com/2020/02/handling-device-orientation-efficiently.html)
bool vkenv_createSwapchain(vkenv_Device device, vkenv_Swapchain *swapchain_ptr, VkSurfaceKHR surface, vkenv_SwapchainPreferences *swapchain_pref);
// Destroy Vulkan entities created during vkenv_createSwapchain() and free any allocated memory
void vkenv_destroySwapchain(vkenv_Device device, vkenv_Swapchain *swapchain_ptr);

#endif // VKENV_SWAPCHAIN_H