#include "vulkan_surface.h"
#include "logger.h"
#include "vulkan_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef VK_USE_PLATFORM_ANDROID_KHR
#include <android/native_window_jni.h>
#endif

static const char LOG_TAG[] = "VulkanSurface";

const char *vkenv_getSurfaceExtensionName()
{
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
#elif VK_USE_PLATFORM_WAYLAND_KHR
  return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#elif VK_USE_PLATFORM_WIN32_KHR
  return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif VK_USE_PLATFORM_XCB_KHR
  return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#elif VK_USE_PLATFORM_XLIB_KHR
  return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#else
  // Should not be compiled if only using compute or headless rendering
#endif
}

bool vkenv_createSurface(VkSurfaceKHR *surface_ptr, vkenv_ExternalWindowInfo *window_info_ptr)
{
  VkResult surface_creation_res = VK_INCOMPLETE; // VK_UNKNOWN not available on Android
#ifdef VK_USE_PLATFORM_ANDROID_KHR

  VkAndroidSurfaceCreateInfoKHR surface_create_info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .pNext = NULL, .flags = 0, .window = *((ANativeWindow **)(window_info_ptr->window))};
  surface_creation_res = vkCreateAndroidSurfaceKHR(vkenv_getInstance(), &surface_create_info, NULL, surface_ptr);

#elif VK_USE_PLATFORM_WIN32_KHR

  VkWin32SurfaceCreateInfoKHR surface_create_info = {.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                                                     .pNext = NULL,
                                                     .flags = 0,
                                                     .hinstance = *((HINSTANCE *)(window_info_ptr->context)),
                                                     .hwnd = *((HWND *)(window_info_ptr->window))};
  surface_creation_res = vkCreateWin32SurfaceKHR(vkenv_getInstance(), &surface_create_info, NULL, surface_ptr);
#elif VK_USE_PLATFORM_XLIB_KHR
  VkXlibSurfaceCreateInfoKHR surface_create_info = {.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                                                    .pNext = NULL,
                                                    .flags = 0,
                                                    .dpy = *((Display **)(window_info_ptr->context)),
                                                    .window = *((Window *)(window_info_ptr->window))};
  surface_creation_res = vkCreateXlibSurfaceKHR(vkenv_getInstance(), &surface_create_info, NULL, surface_ptr);
#else
  // Should not be compiled if only using compute or headless rendering
#endif
  if (surface_creation_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create the VkSurface object (error code: %s)", vkenv_getVkResultString(surface_creation_res));
  }
  return surface_creation_res == VK_SUCCESS;
}
