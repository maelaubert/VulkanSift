#include "debug_presenter.h"
#include "logger.h"
#include "vulkan_surface.h"
#include "vulkan_utils.h"
#include <stdlib.h>
#include <string.h>

static const char LOG_TAG[] = "DebugPresenter";

typedef struct vkenv_DebugPresenter_T
{
  uint32_t target_width;
  uint32_t target_height;
  VkSurfaceKHR surface;
  vkenv_Swapchain swapchain;
  VkCommandPool command_pool;
  VkSemaphore image_available_semaphore;
} vkenv_DebugPresenter_T;

bool createCommandPool(vkenv_Device device, vkenv_DebugPresenter debug_presenter);
bool createSemaphores(vkenv_Device device, vkenv_DebugPresenter debug_presenter);
bool forceSwapchainImagesPresentableState(vkenv_Device device, vkenv_DebugPresenter debug_presenter);
bool recreateSwapchain(vkenv_Device device, vkenv_DebugPresenter debug_presenter);

bool vkenv_createDebugPresenter(vkenv_Device device, vkenv_DebugPresenter *debug_presenter_ptr, vkenv_ExternalWindowInfo *ext_window_info_ptr)
{
  if (debug_presenter_ptr == NULL)
  {
    logError(LOG_TAG, "vkenv_Context_create(): debug_presenter argument must not be NULL");
    return false;
  }
  *debug_presenter_ptr = (vkenv_DebugPresenter)malloc(sizeof(vkenv_DebugPresenter_T));
  vkenv_DebugPresenter debug_presenter = *debug_presenter_ptr;
  memset(debug_presenter, 0, sizeof(vkenv_DebugPresenter_T));

  debug_presenter->target_width = 300;
  debug_presenter->target_height = 100;
  vkenv_SwapchainPreferences swapchain_config = {.width = debug_presenter->target_width, .height = debug_presenter->target_height};

  if (vkenv_createSurface(&debug_presenter->surface, ext_window_info_ptr) &&
      vkenv_createSwapchain(device, &debug_presenter->swapchain, debug_presenter->surface, &swapchain_config) &&
      createCommandPool(device, debug_presenter) && createSemaphores(device, debug_presenter) &&
      forceSwapchainImagesPresentableState(device, debug_presenter))
  {
    return true;
  }
  else
  {
    logError(LOG_TAG, "Failed to create the vkenv_DebugPresenter.");
    vkenv_destroyDebugPresenter(device, debug_presenter_ptr);
    return false;
  }

  return true;
}

bool createCommandPool(vkenv_Device device, vkenv_DebugPresenter debug_presenter)
{
  // Acquire graphics command pool to run GPU commands
  VkCommandPoolCreateInfo command_pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = NULL, .flags = 0, .queueFamilyIndex = device->general_queues_family_idx};

  VkResult create_pool_res = vkCreateCommandPool(device->device, &command_pool_create_info, NULL, &debug_presenter->command_pool);
  if (create_pool_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create graphics command pool (vkCreateCommandPool: %s)", vkenv_getVkResultString(create_pool_res));
    return false;
  }
  return true;
}

bool createSemaphores(vkenv_Device device, vkenv_DebugPresenter debug_presenter)
{
  // Create synchronisation objects
  VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = NULL, .flags = 0};
  VkResult create_semaphore_res = vkCreateSemaphore(device->device, &semaphore_info, NULL, &debug_presenter->image_available_semaphore);
  if (create_semaphore_res != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to create presentation semaphore (vkCreateSemaphore: %d)", vkenv_getVkResultString(create_semaphore_res));
    return false;
  }
  else
  {
    return true;
  }
}

bool forceSwapchainImagesPresentableState(vkenv_Device device, vkenv_DebugPresenter debug_presenter)
{
  // Force swapchain images to the VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layout so they can be presented without seting up a graphics pipeline
  VkCommandBuffer cmd_buf;
  if (!vkenv_beginInstantCommandBuffer(device->device, debug_presenter->command_pool, &cmd_buf))
  {
    logError(LOG_TAG, "Failed to begin the layout switch command buffer.");
    return false;
  }

  for (uint32_t i = 0; i < debug_presenter->swapchain->nb_swapchain_image; i++)
  {
    VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = debug_presenter->swapchain->swapchain_images[i],
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
    vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, NULL, 0, NULL, 1, &image_barrier);
  }
  if (!vkenv_endInstantCommandBuffer(device->device, device->general_queues[0], debug_presenter->command_pool, cmd_buf))
  {
    logError(LOG_TAG, "Failed to submit the layout switch command buffer.");
    return false;
  }

  return true;
}

bool recreateSwapchain(vkenv_Device device, vkenv_DebugPresenter debug_presenter)
{
  vkDeviceWaitIdle(device->device);

  // We don't have any swapchain dependencies so we can directly recreate the swapchain
  vkenv_destroySwapchain(device, &debug_presenter->swapchain);

  vkenv_SwapchainPreferences swapchain_config = {.width = debug_presenter->target_width, .height = debug_presenter->target_height};
  if (!vkenv_createSwapchain(device, &debug_presenter->swapchain, debug_presenter->surface, &swapchain_config) ||
      !forceSwapchainImagesPresentableState(device, debug_presenter))
  {
    return false;
  }

  return true;
}

bool vkenv_presentDebugFrame(vkenv_Device device, vkenv_DebugPresenter debug_presenter)
{
  // Acquire image
  uint32_t image_idx;
  VkResult result = vkAcquireNextImageKHR(device->device, debug_presenter->swapchain->swapchain, UINT64_MAX, debug_presenter->image_available_semaphore,
                                          VK_NULL_HANDLE, &image_idx);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // If VK_ERROR_OUT_OF_DATE_KHR, the surface is no longer compatible with the swapchain, nothing can be displayed so the swapchain must be recreated
    if (!recreateSwapchain(device, debug_presenter))
    {
      logError(LOG_TAG, "Failed to recreate swapchain after VK_ERROR_OUT_OF_DATE_KHR");
      return false;
    }
  }
  // Accept VK_SUBOPTIMAL_KHR because presentation will still be visible (but swapchain should be recreated after presentation)
  else if (result != VK_SUBOPTIMAL_KHR && result != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to acquire acquire next swapchain image before drawing");
    return false;
  }

  // Present as soon as image is ready
  VkPresentInfoKHR present_info = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &debug_presenter->image_available_semaphore,
                                   .swapchainCount = 1,
                                   .pSwapchains = &debug_presenter->swapchain->swapchain,
                                   .pImageIndices = &image_idx,
                                   .pResults = NULL};
  result = vkQueuePresentKHR(device->general_queues[0], &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
  {
    if (!recreateSwapchain(device, debug_presenter))
    {
      logError(LOG_TAG, "Failed to recreate swapchain after VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR");
      return false;
    }
  }
  else if (result != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to present swapchain image");
    return false;
  }

  return true;
}

void vkenv_destroyDebugPresenter(vkenv_Device device, vkenv_DebugPresenter *debug_presenter_ptr)
{
  if (debug_presenter_ptr == NULL)
  {
    logError(LOG_TAG, "vkenv_destroyDebugPresenter() called with a NULL vkenv_DebugPresenter pointer");
    return;
  }
  if (*debug_presenter_ptr == NULL)
  {
    logError(LOG_TAG, "vkenv_destroyDebugPresenter() called with a NULL vkenv_DebugPresenter");
    return;
  }
  vkenv_DebugPresenter debug_presenter = *debug_presenter_ptr;

  // Wait for any current work to finish
  vkDeviceWaitIdle(device->device);

  // If swapchain still extists destroy it and its direct dependencies
  if (debug_presenter->swapchain != NULL)
  {
    // Destroy swapchain
    vkenv_destroySwapchain(device, &debug_presenter->swapchain);
  }

  // Destroy command buffers (destroying a command pool also deallocate its buffers)
  VK_NULL_SAFE_DELETE(debug_presenter->command_pool, vkDestroyCommandPool(device->device, debug_presenter->command_pool, NULL));

  // Destroy swapchain image available semaphore
  VK_NULL_SAFE_DELETE(debug_presenter->image_available_semaphore, vkDestroySemaphore(device->device, debug_presenter->image_available_semaphore, NULL));

  // Destroy surface
  VK_NULL_SAFE_DELETE(debug_presenter->surface, vkDestroySurfaceKHR(vkenv_getInstance(), debug_presenter->surface, NULL));

  free(*debug_presenter_ptr);
  *debug_presenter_ptr = NULL;
}