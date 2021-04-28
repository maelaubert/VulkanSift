#ifndef VULKAN_INSTANCE_H
#define VULKAN_INSTANCE_H

#ifndef VK_USE_PLATFORM_ANDROID_KHR
#include "vulkansift/utils/GLFW_loader.h"
#endif

#include "vulkansift/utils/vulkan_loader.h"
#include <vector>
//#include <vulkan/vulkan.h>

class VulkanInstance
{

  public:
  VulkanInstance() = default;

// If running on Android, we don't need to create a custom window and can use
// the provided one
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  bool init(ANativeWindow *window, const int window_width, const int window_height, bool use_validation_layers, bool use_debug_extensions);
#else
  bool init(const int window_width, const int window_height, bool use_validation_layers, bool use_debug_extensions);
#endif
  bool shouldStop();
  bool resetSwapchain();
  void terminate();

  // Accessors
  VkDevice getVkDevice() { return m_device; }
  VkPhysicalDevice getVkPhysicalDevice() { return m_physical_device; }
  uint32_t getGraphicsQueueFamilyIndex() { return m_general_queue_family_index; }
  VkQueue getGraphicsQueue() { return m_general_queue; }
  bool isAsyncComputeAvailable() { return m_async_compute_available; }
  uint32_t getAsyncComputeQueueFamilyIndex() { return m_async_compute_queue_family_index; }
  VkQueue getAsyncComputeQueue() { return m_async_compute_queue; }

  VkRenderPass getRenderPass() { return m_render_pass; }
  VkSwapchainKHR getSwapchain() { return m_swapchain; }
  VkExtent2D getSwapchainExtent() { return m_swapchain_extent; }
  uint32_t getNbSwapchainImage() { return m_nb_swapchain_image; }
  std::vector<VkFramebuffer> getSwapchainFramebuffers() { return m_swapchain_framebuffers; }

  // Vulkan device info
  VkPhysicalDeviceProperties getVkPhysicalDeviceProperties() { return m_physical_device_props; }

  // Timestamps related functions
  bool isTimestampQuerySupported() { return m_ts_query_supported; }
  float getTimestampQueryPeriodNs() { return m_ts_query_period; }
  uint64_t getTimestampBitMask() { return m_ts_bitmask; }

  // EXT_debug functions
  bool isDebugMarkerSupported() { return m_debug_funcs_available; }
  PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
  PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
  PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;

  private:
  bool createInstance();

  // Get the surface used for drawing
  bool createSurface();

  bool findPhysicalDevice();
  bool isPhysicalDeviceValid(const VkPhysicalDevice &device);
  bool createLogicalDevice();

  // Entites related to surface acquisition
  bool createSwapchain();
  bool createSwaphainImageViews();
  bool createRenderPass();
  bool createFramebuffers();

  // CLEANUP
  void cleanupVulkanEntities();
  void cleanupSwapchain();

// Window type depends on current platform
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  ANativeWindow *m_android_native_window = nullptr;
#else
  GLFWwindow *m_glfw_window = nullptr;
#endif
  const std::vector<const char *> m_validation_layers = {"VK_LAYER_KHRONOS_validation"};
  bool m_validation_layer_enabled = false;
  bool m_debug_ext_enabled = false;
  bool m_debug_funcs_available = false;

  const std::vector<const char *> m_fixed_instance_extensions = {};
  const std::vector<const char *> m_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  int m_window_width;
  int m_window_height;

  VkInstance m_instance = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;

  uint32_t m_general_queue_family_index;
  VkQueue m_general_queue = VK_NULL_HANDLE;
  bool m_async_compute_available = false;
  uint32_t m_async_compute_queue_family_index;
  VkQueue m_async_compute_queue = VK_NULL_HANDLE;

  VkSurfaceCapabilitiesKHR m_swapchain_capabilities;
  std::vector<VkSurfaceFormatKHR> m_swapchain_available_formats;
  std::vector<VkPresentModeKHR> m_swapchain_available_present_modes;

  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  VkFormat m_swapchain_format;
  VkExtent2D m_swapchain_extent;
  uint32_t m_nb_swapchain_image;
  std::vector<VkImage> m_swapchain_images;
  std::vector<VkImageView> m_swapchain_image_views;

  VkRenderPass m_render_pass = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> m_swapchain_framebuffers;

  VkPhysicalDeviceProperties m_physical_device_props;

  bool m_ts_query_supported = false;
  float m_ts_query_period = 0.f;
  uint64_t m_ts_bitmask = 0u;
};

#endif // VULKAN_INSTANCE_H