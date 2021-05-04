#ifndef VKSIFT_SIFTMEMORY
#define VKSIFT_SIFTMEMORY

#include "types.h"
#include "vkenv/vulkan_device.h"

typedef struct
{
  bool is_busy;
  bool is_packed;
  uint32_t curr_input_width;
  uint32_t curr_input_height;
  uint32_t *octave_section_size;
} vksift_SiftBufferInfo;

typedef struct vksift_SiftMemory_T
{
  VkCommandPool general_command_pool;

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
  VkBuffer sift_count_staging_buffer;
  VkDeviceMemory sift_count_staging_buffer_memory;
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
  VkDeviceSize intput_image_memory_size;

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
  uint8_t curr_nb_octaves;
  uint32_t *octave_resolutions;

  // Matching buffers
  VkBuffer match_output_buffer;
  VkDeviceMemory match_output_buffer_memory;

  VkBuffer match_output_staging_buffer;
  VkDeviceMemory match_output_staging_buffer_memory;
  void *match_output_staging_buffer_ptr;

  // Other
  VkBuffer indirect_dispatch_buffer;
  VkDeviceMemory indirect_dispatch_buffer_memory;

  // Config
  uint32_t max_image_size;
  uint8_t max_nb_octaves;
  uint8_t nb_scales_per_octave;
  uint32_t nb_sift_buffer;
  uint32_t max_nb_sift_per_buffer;
  vksift_PyramidPrecisionMode pyr_precision_mode;
  bool use_upsampling;
} * vksift_SiftMemory;

// Setup every memory object
// Create image/buffers with the maximum size requirements for memory allocation
// Map staging in and staging out buffers (input image, output image, input buffer, output buffer)
bool vksift_createSiftMemory(vkenv_Device device, vksift_SiftMemory *memory_ptr, const vksift_Config *config);

// Need to recompute the octave resolution (and number) and recreate+bind the images
// Since the images will be new GPU objects, the related descriptors must be updated
// If nb octave updated must update SIFT buffers info (sections offset and sizes)
// Need to setup image layout
bool vksift_prepareForInputResolution(vkenv_Device device, vksift_SiftMemory memory, const uint32_t target_buffer_idx, const uint32_t input_width,
                                      const uint32_t input_height);
// Destory every memory object and free stuffs
void vksift_destroySiftMemory(vkenv_Device device, vksift_SiftMemory *memory_ptr);

#endif // VKSIFT_SIFTMEMORY