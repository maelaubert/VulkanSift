#ifndef VKENV_SURFACE_H
#define VKENV_SURFACE_H

#include "vulkan_device.h"

typedef struct
{
  // Target pointer should depend on the targeted window system:
  //    - XLIB: context is Display**, window is Window*
  //    - WIN32: context is HINSTANCE*, window is HWND*
  //    - ANDROID: context should be NULL, window is ANativeWindow**
  //    - MacOS/iOS: context should be NULL, window is CAMetalLayer**
  void *context;
  void *window;
} vkenv_ExternalWindowInfo;

const char *vkenv_getSurfaceExtensionName();
bool vkenv_createSurface(VkSurfaceKHR *surface_ptr, vkenv_ExternalWindowInfo *window_info_ptr);

#endif // VKENV_SURFACE_H