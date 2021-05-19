#ifndef VKSIFT_SIFTMEMORY
#define VKSIFT_SIFTMEMORY

#include "types.h"
#include "vkenv/vulkan_device.h"

typedef struct
{
  bool is_packed;
  uint32_t nb_stored_feats; // only available if is_packed is true

  uint32_t curr_input_width;
  uint32_t curr_input_height;
  // Number of SIFT feature per sections
  uint32_t *octave_section_max_nb_feat_arr;
  // Offset to get the start of the sections
  VkDeviceSize *octave_section_offset_arr;
  // Section sizes
  VkDeviceSize *octave_section_size_arr;
} vksift_SiftBufferInfo;

typedef struct
{
  uint32_t width;
  uint32_t height;
} vksift_OctaveResolution;

typedef struct vksift_SiftMemory_T
{
  vkenv_Device device; // parent device

  VkCommandPool general_command_pool;
  VkCommandPool async_transfer_command_pool;
  VkCommandBuffer transfer_command_buffer;

  VkFence transfer_fence;

  // SIFT buffers

  // Info defining how the data is arranged inside the buffer
  // [buff1_is_packed=0u, buff1_oct_1_size=X, buff1_oct_2_size=X, ..., buff2_is_packed=0u, buff2_oct_1_size=X, buff2_oct_2_size=X, ...]
  // If a buffer is not packed, sift features will be arranged in independent section and the number of sift detected for a given octave will be limited
  // by the section size. If the buffer is packed then all the section have been packed to the left of the buffer, there will be no free space
  // between the sift features of different octaves.
  // The section structure depends on the number of octaves, so it depends on the input image resolution and must be recomputed for every input.
  vksift_SiftBufferInfo *sift_buffers_info; // nb_sift_buffer * (max_nb_octaves+1) * sizeof(uint32_t)
  VkBuffer *sift_buffer_arr;
  VkDeviceMemory *sift_buffer_memory_arr;
  VkBuffer *sift_count_staging_buffer_arr;
  VkDeviceMemory *sift_count_staging_buffer_memory_arr;
  void **sift_count_staging_buffer_ptr_arr;
  VkBuffer sift_staging_buffer;
  VkDeviceMemory sift_staging_buffer_memory;
  void *sift_staging_buffer_ptr;

  // Pyramid objects
  VkBuffer image_staging_buffer;
  VkDeviceMemory image_staging_buffer_memory;
  void *image_staging_buffer_ptr;

  VkImage input_image;
  VkImageView input_image_view;
  VkDeviceMemory input_image_memory;
  VkDeviceSize input_image_memory_size;

  VkImage output_image; // output image is used to export scalespace images to the CPU for debug/viz
  VkDeviceMemory output_image_memory;

  VkImage *octave_image_arr;
  VkImageView *octave_image_view_arr;
  VkDeviceMemory *octave_image_memory_arr;
  VkDeviceSize *octave_image_memory_size_arr;

  VkImage *blur_tmp_image_arr;
  VkImageView *blur_tmp_image_view_arr;
  VkDeviceMemory *blur_tmp_image_memory_arr;
  VkDeviceSize *blur_tmp_image_memory_size_arr;

  VkImage *octave_DoG_image_arr;
  VkImageView *octave_DoG_image_view_arr;
  VkDeviceMemory *octave_DoG_image_memory_arr;
  VkDeviceSize *octave_DoG_image_memory_size_arr;

  // Pyramid info
  uint32_t curr_input_image_width;
  uint32_t curr_input_image_height;
  uint32_t curr_nb_octaves;
  vksift_OctaveResolution *octave_resolutions;

  // Matching buffers
  uint32_t curr_nb_matches; // simply updated using the buffer A number of features
  VkBuffer match_output_buffer;
  VkDeviceMemory match_output_buffer_memory;

  VkBuffer match_output_staging_buffer;
  VkDeviceMemory match_output_staging_buffer_memory;
  void *match_output_staging_buffer_ptr;

  // Other
  VkDeviceSize *indirect_oridesc_offset_arr;
  VkBuffer indirect_orientation_dispatch_buffer;
  VkDeviceMemory indirect_orientation_dispatch_buffer_memory;
  VkBuffer indirect_descriptor_dispatch_buffer;
  VkDeviceMemory indirect_descriptor_dispatch_buffer_memory;
  VkBuffer indirect_matcher_dispatch_buffer;
  VkDeviceMemory indirect_matcher_dispatch_buffer_memory;

  VkQueue general_queue;
  VkQueue async_transfer_queue;

  // Config
  uint32_t max_image_size;
  uint32_t max_nb_octaves;
  uint32_t nb_scales_per_octave;
  uint32_t nb_sift_buffer;
  uint32_t max_nb_sift_per_buffer;
  vksift_PyramidPrecisionMode pyr_precision_mode;
  bool use_upsampling;
} * vksift_SiftMemory;

// Setup every memory object
// Create image/buffers with the maximum size requirements for memory allocation
// Map staging in and staging out buffers (input image, output image, input buffer, output buffer)
bool vksift_createSiftMemory(vkenv_Device device, vksift_SiftMemory *memory_ptr, const vksift_Config *config);

// Recompute the octave resolution (and number) and recreate+bind the images
// Since the images will be new GPU objects, the related descriptors in the computing pipelines must be updated
bool vksift_prepareSiftMemoryForDetection(vksift_SiftMemory memory, const uint8_t *image_data, const uint32_t input_width, const uint32_t input_height,
                                          const uint32_t target_buffer_idx, bool *memory_layout_updated);

// Update the buffer structure to a packed format with a 2-uint32 header (containing the number of features) and all the features aligned
// after this header (requirement for the matching pipeline)
bool vksift_prepareSiftMemoryForMatching(vksift_SiftMemory memory, const uint32_t target_buffer_A_idx, const uint32_t target_buffer_B_idx);

// Read from staging buffer memory to retrieve the number of features stored in a SIFT buffer (GPU not involved in this function)
bool vksift_Memory_getBufferFeatureCount(vksift_SiftMemory memory, const uint32_t target_buffer_idx, uint32_t *out_feat_count);

// Run a transfer command to retrieve the SIFT buffer features from the GPU (run on the asynchronous transfer queue if available)
bool vksift_Memory_copyBufferFeaturesFromGPU(vksift_SiftMemory memory, const uint32_t target_buffer_idx, vksift_Feature *out_features_ptr);

// Run a transfer command to transfer user SIFT features to the SIFT buffer on the GPU (run on the asynchronous transfer queue if available)
bool vksift_Memory_copyBufferFeaturesToGPU(vksift_SiftMemory memory, const uint32_t target_buffer_idx, vksift_Feature *in_features_ptr,
                                           const uint32_t in_feat_count);

// Read from staging buffer memory to retrieve the number of features matches stored in a matches buffer (GPU not involved in this function)
bool vksift_Memory_getBufferMatchesCount(vksift_SiftMemory memory, uint32_t *out_matches_count);

// Run a transfer command to retrieve the SIFT matches from the GPU (GPU not involved in this function)
bool vksift_Memory_copyBufferMatchesFromGPU(vksift_SiftMemory memory, vksift_Match_2NN *out_matches_ptr);

// Transfer one of the pyramid image to the CPU
bool vksift_Memory_copyPyramidImageFromGPU(vksift_SiftMemory memory, const uint8_t octave, const uint8_t scale, const bool is_dog, float *out_image_data);

// Destory every memory object and free stuffs
void vksift_destroySiftMemory(vksift_SiftMemory *memory_ptr);

#endif // VKSIFT_SIFTMEMORY