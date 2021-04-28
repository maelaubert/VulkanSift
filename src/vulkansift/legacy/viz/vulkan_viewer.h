#ifndef VULKAN_VIEWER_H
#define VULKAN_VIEWER_H

#include "vulkansift/utils/vulkan_utils.h"
#include "vulkansift/viz/vulkan_viewer.h"
#include "vulkansift/vulkan_instance.h"

#ifdef VK_USE_PLATFORM_ANDROID_KHR
#include <android/asset_manager.h>
#endif

class VulkanViewer
{
  public:
  VulkanViewer() = default;
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  bool init(VulkanInstance *vulkan_instance, AAssetManager *asset_manager, const int image_width, const int image_height);
#else
  bool init(VulkanInstance *vulkan_instance, const int image_width, const int image_height);
#endif
  bool execOnce(uint8_t *pixel_buffer, float *gpu_time_ms);
  bool shouldStop() { return m_vulkan_instance->shouldStop(); }
  void terminate();

  private:
  // Application specific part (could be moved in a different class, would need access to configurable VulkanInstance)

  // GRAPHICS
  bool createGraphics();
  // Create command pool for graphics operations
  bool createGraphicsCommandPool();
  // Handle graphics related buffers memory allocation (single physical memory if possible)
  // Create vertex and index buffers (for input ), copy using graphics command pool
  bool createGraphicsBuffers();

  // Create descriptor layout (set binding info for UBO,SSBO,texture,image needed at vertex stage)
  // Create descriptor pool
  // Allocate descriptor sets from descriptor pool (one descriptor set for every swapchain image for non concurrent access)
  bool createGraphicsDescriptors();
  // Create graphics pipeline (linked to descriptor layout only)
  //  - Setup VertexInput properly with bindings and attributes according to what's in the vertex buffer
  bool createGraphicsPipeline();
  // Allocate command buffer from the command pool (one command buffer for one swapchain image) and then
  // fill each buffer with commands: in order BeginCommandBuf, BeginRenderPass,
  //                                          BindPipeline, BindVertex, BindIndex, BindDescriptors, Draw
  //                                          EndRenderPass, EndCommandBuf
  bool createGraphicsCommandBuffers();
  // Create synchronization objects used at execution time for the graphics part
  bool createGraphicsSyncObjects();

  bool createImages();
  bool createSamplers();

  // Compute
  bool setupQueries();

  // CLEANUP
  bool recreateSwapchain();
  void cleanupGraphics();

  const std::vector<float> m_graphics_vertices{-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,  //
                                               1.0f,  -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,  //
                                               1.0f,  1.0f,  0.0f, 0.0f, 1.0,  1.0f, 1.0f,  //
                                               -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f}; //
  const std::vector<uint32_t> m_graphics_indices = {0, 1, 2, 2, 3, 0};

  uint32_t m_image_width;
  uint32_t m_image_height;
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  AAssetManager *m_asset_manager;
#endif
  VulkanInstance *m_vulkan_instance = nullptr;
  // Retrieved from VulkanInstance
  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  uint32_t m_queue_family_index;
  VkQueue m_queue = VK_NULL_HANDLE;
  VkRenderPass m_render_pass = VK_NULL_HANDLE;
  uint32_t m_nb_swapchain_image;
  VkExtent2D m_swapchain_extent;
  std::vector<VkFramebuffer> m_swapchain_framebuffers;
  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

  // Application
  // GRAPHICS
  VkCommandPool m_graphics_command_pool = VK_NULL_HANDLE;
  VulkanUtils::Buffer m_graphics_buffer;
  VkDescriptorSetLayout m_graphics_desc_set_layout = VK_NULL_HANDLE;
  VkDescriptorPool m_graphics_desc_pool = VK_NULL_HANDLE;
  VkDescriptorSet m_graphics_desc_set = VK_NULL_HANDLE;

  VkPipelineLayout m_graphics_pipeline_layout = VK_NULL_HANDLE;
  VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;

  std::vector<VkCommandBuffer> m_graphics_command_buffers;

  VkSemaphore m_graphics_image_available_semaphore = VK_NULL_HANDLE;
  VkSemaphore m_graphics_render_finished_semaphore = VK_NULL_HANDLE;
  VkFence m_frame_rendering_fence = VK_NULL_HANDLE;

  // Images
  VulkanUtils::Buffer m_input_img_staging_buffer;
  VulkanUtils::Image m_input_image;
  VkSampler m_texture_sampler = VK_NULL_HANDLE;

  // Queries
  VkQueryPool m_ts_query_pool = VK_NULL_HANDLE;
};

#endif // VULKAN_VIEWER_H