#ifndef VKENV_DEBUG_PRESENTER_H
#define VKENV_DEBUG_PRESENTER_H

#include "vulkan_device.h"
#include "vulkan_surface.h"
#include "vulkan_swapchain.h"

typedef struct vkenv_DebugPresenter_T *vkenv_DebugPresenter;

// DebugPresenter is a debug tool for compute only applications.
// Lots of Vulkan debugger/profiler are only targetting graphics applications and provide information per-frame.
// They use the rendering or surface presentation commands as frame delimiters so they can't work on application not using these commands.
// DebugPresenter is a minimal app that handles seting up a rendering environment (window, surface and swapchain creation)
// and present empty frames to help debugging compute only applications on GPUs supporting graphics operations.
bool vkenv_createDebugPresenter(vkenv_Device device, vkenv_DebugPresenter *debug_presenter_ptr, vkenv_ExternalWindowInfo *ext_window_info_ptr);

// A "frame delimiter" is placed after every call to vkenv_presentDebugFrame(). This function return false if an error
// occured during the frame presentation or if the user/system closed the window.
// Code to debug/profile can be easily wrapped inside a while(vkenv_presentDebugFrame()) loop.
bool vkenv_presentDebugFrame(vkenv_Device device, vkenv_DebugPresenter debug_presenter);
void vkenv_destroyDebugPresenter(vkenv_Device device, vkenv_DebugPresenter *debug_presenter_ptr);

#endif // VKENV_DEBUG_PRESENTER_H