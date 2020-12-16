#ifndef SIFT_DETECTOR_H
#define SIFT_DETECTOR_H

#include "vulkansift/sift/sift_feature.h"
#include "vulkansift/utils/vulkan_utils.h"
#include "vulkansift/vulkan_instance.h"

#include <cmath>

namespace VulkanSIFT
{

class SiftDetector
{
  public:
  SiftDetector() = default;
  bool init(VulkanInstance *vulkan_instance, const int image_width, const int image_height);
  bool compute(uint8_t *pixel_buffer, std::vector<SIFT_Feature> &sift_feats);
  void terminate();

  private:
  bool initCommandPool();
  bool initMemory();
  bool initDescriptors();
  bool initPipelines();
  bool initCommandBuffer();

  void beginMarkerRegion(VkCommandBuffer cmd_buf, const char *region_name);
  void endMarkerRegion(VkCommandBuffer cmd_buf);

  // VulkanManager related
  VulkanInstance *m_vulkan_instance = nullptr;
  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkQueue m_queue = VK_NULL_HANDLE;
  uint32_t m_queue_family_index = 0u;
  VkQueue m_async_queue = VK_NULL_HANDLE;
  uint32_t m_async_queue_family_index = 0u;
  bool m_debug_marker_supported = false;

  ///////////////////////////////////////////////////////////////////////////////
  /* SIFT PARAMETERS */
  // Parameters for scale space
  uint32_t m_nb_octave = 5;
  // int nb_oct = roundf(logf(static_cast<float>(MIN(width_in*2, height_in*2))) / logf(2.f) - 2) + 1;

  uint32_t m_nb_scale_per_oct = 3;
  float m_sigma_min = 0.8f;
  float m_sigma_in = 0.5f;
  float m_scale_factor_min = 0.5f;
  // Parameters for keypoints detection
  float m_unscaled_dog_threshold = 0.015f;
  // float unscaled_dog_threshold = 0.04f/3.f;
  float m_dog_threshold = ((powf(2.f, 1.f / m_nb_scale_per_oct) - 1.f) / (powf(2.f, 1.f / 3.f) - 1.f)) * m_unscaled_dog_threshold;
  // float dog_threshold = unscaled_dog_threshold;
  float m_soft_dog_threshold = m_dog_threshold * 0.8f;
  // float soft_dog_threshold = 0.0039;//dog_threshold * 0.5f;
  // float soft_dog_threshold = dog_threshold * 0.5f;

  uint32_t m_kp_refinement_nb_steps = 5;
  float m_kp_edge_threshold = 10.f;
  float m_min_distance_to_border = 1.f;
  // Parameters for keypoints orientation computation
  float m_lambda_orientation = 1.5f;
  uint32_t m_nb_bins = 36;
  float m_local_extrema_threshold = 0.8f;
  // Parameters for SIFT feature computation
  float m_lambda_descriptor = 6.f;
  float m_l2_norm_threshold = 0.2f;
  ///////////////////////////////////////////////////////////////////////////////

  uint32_t m_image_width;
  uint32_t m_image_height;
  ////////////////////
  // Memory objects
  //
  // Images
  struct ImageSize
  {
    uint32_t width;
    uint32_t height;
  };
  std::vector<ImageSize> m_octave_image_sizes;
  VulkanUtils::Image m_input_image;
  std::vector<VulkanUtils::Image> m_blur_temp_results;
  std::vector<VulkanUtils::Image> m_octave_images;
  std::vector<VulkanUtils::Image> m_octave_DoG_images;
  // Buffers
  VulkanUtils::Buffer m_input_image_staging_in_buffer;
  std::vector<VulkanUtils::Buffer> m_sift_staging_out_buffers;
  uint32_t m_sift_buff_max_elem = 20000;
  std::vector<VulkanUtils::Buffer> m_sift_keypoints_buffers;
  std::vector<VulkanUtils::Buffer> m_indispatch_buffers;

  ////////////////////

  ////////////////////
  // Commands related
  ////////////////////
  VkCommandBuffer m_command_buffer;
  VkCommandPool m_command_pool;

  ////////////////////
  // Pipelines and descriptors
  ////////////////////
  // GaussianBlur
  VkDescriptorSetLayout m_blur_desc_set_layout = VK_NULL_HANDLE;
  VkDescriptorPool m_blur_desc_pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> m_blur_h_desc_sets;
  std::vector<VkDescriptorSet> m_blur_v_desc_sets;
  VkPipelineLayout m_blur_pipeline_layout = VK_NULL_HANDLE;
  VkPipeline m_blur_pipeline = VK_NULL_HANDLE;
  // DifferenceOfGaussian
  VkDescriptorSetLayout m_dog_desc_set_layout = VK_NULL_HANDLE;
  VkDescriptorPool m_dog_desc_pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> m_dog_desc_sets;
  VkPipelineLayout m_dog_pipeline_layout = VK_NULL_HANDLE;
  VkPipeline m_dog_pipeline = VK_NULL_HANDLE;
  // ExtractKeypoints
  VkDescriptorSetLayout m_extractkpts_desc_set_layout = VK_NULL_HANDLE;
  VkDescriptorPool m_extractkpts_desc_pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> m_extractkpts_desc_sets;
  VkPipelineLayout m_extractkpts_pipeline_layout = VK_NULL_HANDLE;
  VkPipeline m_extractkpts_pipeline = VK_NULL_HANDLE;
  // ComputeOrientation
  VkDescriptorSetLayout m_orientation_desc_set_layout = VK_NULL_HANDLE;
  VkDescriptorPool m_orientation_desc_pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> m_orientation_desc_sets;
  VkPipelineLayout m_orientation_pipeline_layout = VK_NULL_HANDLE;
  VkPipeline m_orientation_pipeline = VK_NULL_HANDLE;
  // ComputeDescriptor
  VkDescriptorSetLayout m_descriptor_desc_set_layout = VK_NULL_HANDLE;
  VkDescriptorPool m_descriptor_desc_pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> m_descriptor_desc_sets;
  VkPipelineLayout m_descriptor_pipeline_layout = VK_NULL_HANDLE;
  VkPipeline m_descriptor_pipeline = VK_NULL_HANDLE;

  void *m_input_image_ptr = nullptr;
  std::vector<void *> m_output_sift_ptr;

  VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace VulkanSIFT

#endif // SIFT_DETECTOR_H