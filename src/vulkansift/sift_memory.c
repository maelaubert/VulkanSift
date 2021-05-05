#include "sift_memory.h"

#include "vkenv/logger.h"
#include "vkenv/vulkan_utils.h"

#include "vulkansift/types.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char LOG_TAG[] = "SiftMemory";

void updateScaleSpaceInfo(vksift_SiftMemory memory)
{
  // Update current number of octave
  uint32_t lowest_dim =
      (memory->curr_input_image_width > memory->curr_input_image_height) ? memory->curr_input_image_height : memory->curr_input_image_width;

  // Max number of octave for the input resolution such that the lowest dimension on the smallest octave is not less than 16 pixels
  memory->curr_nb_octaves = log2f((float)lowest_dim) - 4 + (memory->use_upsampling ? 2 : 1);
  // If the maximum number of octaves is more than the number of octave allowed by the configuration, use the smallest number
  if (memory->max_nb_octaves < memory->curr_nb_octaves)
  {
    memory->curr_nb_octaves = memory->max_nb_octaves;
  }

  // Update octave resolutions
  float scale_factor = (memory->use_upsampling ? 0.5f : 1.f);
  for (uint32_t oct_idx = 0; oct_idx < memory->curr_nb_octaves; oct_idx++)
  {
    // Compute octave images width and height
    memory->octave_resolutions[oct_idx].width = (1.f / (powf(2.f, oct_idx) * scale_factor)) * (float)memory->curr_input_image_width;
    memory->octave_resolutions[oct_idx].height = (1.f / (powf(2.f, oct_idx) * scale_factor)) * (float)memory->curr_input_image_height;
    logInfo(LOG_TAG, "Octave %d resolution: (%d, %d)", oct_idx, memory->octave_resolutions[oct_idx].width, memory->octave_resolutions[oct_idx].height);
  }
}

void updateBufferInfo(vksift_SiftMemory memory, uint32_t buffer_idx)
{
  // This function is called before computing SIFT results on a new image with this buffer idx as a target buffer
  // The buffer is always "not busy" and resetted to non-packed in this case
  memory->sift_buffers_info[buffer_idx].is_busy = false;
  memory->sift_buffers_info[buffer_idx].is_packed = false;
  // Set last input resolution used to the pyramid input resolution
  memory->sift_buffers_info[buffer_idx].curr_input_width = memory->curr_input_image_width;
  memory->sift_buffers_info[buffer_idx].curr_input_height = memory->curr_input_image_height;
  // The number of section in the SIFT buffer depends only on the number of octave used for the input image
  // A bit of explanation here... we define sections such that the number of SIFT features memory dedicated to an octave is half of the memory
  // dedicated to the features on the octave above (with octave images 2x bigger than this octave's images) and we want the total sum of feature memory
  // per section to be the max number of feature stored in the buffer. We consider that since each octave is 2x smaller that the one above there
  // will be 2x less feature detected
  // [            SECTION0            ][    SECTION1    ][SECTION2] with SECTION0+SECTION1+SECTION2 = max_nb_sift_per_buffer
  // To find the number of dedicated feature per buffer we use that fact the sum of the n successive halves of x converse to x when n goes to infinity.
  // We use the first nb_octave halves of max_nb_sift_per_buffer, since the number of octave is usually small the sum halves_sum will not be close to
  // max_nb_sift_per_buffer, so we multiply each half by (max_nb_sift_per_buffer/halves_sum) to correct for the difference.
  // example: max_nb_sift_per_buffer = 1000, nb_octave = 3, halves = [500,250,125] with sum 875, we obtain the following sections
  // [500* (1000/875), 250* (1000/875), 125* (1000/875)] = [571.42, 285.71, 142.85] with sum 1000

  // Any not used octave (among the max number of octave) will have its size set to 0
  memset(memory->sift_buffers_info[buffer_idx].octave_section_max_nb_feat_arr, 0, sizeof(uint32_t) * memory->max_nb_octaves);
  float max_nb_sift_in_buff = (float)memory->max_nb_sift_per_buffer;
  // The sum of n successive halves of X = X - nth_half (500+250+125 = 1000-125)
  float halves_sum = max_nb_sift_in_buff - powf(0.5, memory->curr_nb_octaves) * max_nb_sift_in_buff;
  float corrector = max_nb_sift_in_buff / halves_sum;
  for (uint32_t i = 0; i < memory->curr_nb_octaves; i++)
  {
    memory->sift_buffers_info[buffer_idx].octave_section_max_nb_feat_arr[i] = (uint32_t)floorf((powf(0.5, i + 1) * max_nb_sift_in_buff) * corrector);
    logInfo(LOG_TAG, "Octave %d max number of features: %d", i, memory->sift_buffers_info[buffer_idx].octave_section_max_nb_feat_arr[i]);
  }
}

bool setupDynamicObjectsAndMemory(vksift_SiftMemory memory)
{
  // Setup Pyramid related objects (must be updated when the input resolution changes)
  // Memory is only allocated on first call or if the previous allocation isn't large enough, this should not happen (or very rarely due to driver decision
  // on the alignment) since on first call the memory is allocated to support max size items at runtime.

  VkFormat pyramid_format = (memory->pyr_precision_mode == VKSIFT_PYRAMID_PRECISION_FLOAT16) ? VK_FORMAT_R16_SFLOAT : VK_FORMAT_R32_SFLOAT;

  bool res;
  VkMemoryRequirements memory_requirement;
  uint32_t memory_type_idx;

  // Create input image and image view
  res = true;
  res = res && vkenv_createImage(&memory->input_image, memory->device, 0, VK_IMAGE_TYPE_2D, VK_FORMAT_R8_UNORM,
                                 (VkExtent3D){.width = memory->curr_input_image_width, .height = memory->curr_input_image_height, .depth = 1}, 1, 1,
                                 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE,
                                 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED);

  vkGetImageMemoryRequirements(memory->device->device, memory->input_image, &memory_requirement);
  if (memory_requirement.size > memory->intput_image_memory_size)
  {
    VK_NULL_SAFE_DELETE(memory->input_image_memory, vkFreeMemory(memory->device->device, memory->input_image_memory, NULL));
    res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
    res = res && vkenv_allocateMemory(&memory->input_image_memory, memory->device, memory_requirement.size, memory_type_idx);
    memory->intput_image_memory_size = memory_requirement.size;
    logInfo(LOG_TAG, "Input image (%d,%d) realloc", memory->curr_input_image_width, memory->curr_input_image_height);
  }
  res = res && vkenv_bindImageMemory(memory->device, memory->input_image, memory->input_image_memory, 0u);
  res = res && vkenv_createImageView(&memory->input_image_view, memory->device, 0, memory->input_image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM,
                                     VKENV_DEFAULT_COMPONENT_MAPPING, (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the input image");
    return false;
  }

  // Create blur temp result images (one per octave)
  res = true;
  for (uint32_t oct_idx = 0; oct_idx < memory->max_nb_octaves; oct_idx++)
  {
    uint32_t width = memory->octave_resolutions[oct_idx].width;
    uint32_t height = memory->octave_resolutions[oct_idx].height;
    res = res &&
          vkenv_createImage(&memory->blur_tmp_image_arr[oct_idx], memory->device, 0, VK_IMAGE_TYPE_2D, pyramid_format,
                            (VkExtent3D){.width = width, .height = height, .depth = 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED);

    vkGetImageMemoryRequirements(memory->device->device, memory->blur_tmp_image_arr[oct_idx], &memory_requirement);
    if (memory_requirement.size > memory->blur_tmp_image_memory_size_arr[oct_idx])
    {
      VK_NULL_SAFE_DELETE(memory->blur_tmp_image_memory_arr[oct_idx],
                          vkFreeMemory(memory->device->device, memory->blur_tmp_image_memory_arr[oct_idx], NULL));
      res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
      res = res && vkenv_allocateMemory(&memory->blur_tmp_image_memory_arr[oct_idx], memory->device, memory_requirement.size, memory_type_idx);
      memory->blur_tmp_image_memory_size_arr[oct_idx] = memory_requirement.size;
      logInfo(LOG_TAG, "Blur tmp image (oct %d) (%d,%d) realloc", oct_idx, width, height);
    }
    res = res && vkenv_bindImageMemory(memory->device, memory->blur_tmp_image_arr[oct_idx], memory->blur_tmp_image_memory_arr[oct_idx], 0);
    res = res && vkenv_createImageView(&memory->blur_tmp_image_view_arr[oct_idx], memory->device, 0, memory->blur_tmp_image_arr[oct_idx],
                                       VK_IMAGE_VIEW_TYPE_2D_ARRAY, pyramid_format, VKENV_DEFAULT_COMPONENT_MAPPING,
                                       (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  }
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the temporary blur result images");
    return false;
  }

  // Create gaussian image array per octave
  res = true;
  for (uint32_t oct_idx = 0; oct_idx < memory->max_nb_octaves; oct_idx++)
  {
    uint32_t width = memory->octave_resolutions[oct_idx].width;
    uint32_t height = memory->octave_resolutions[oct_idx].height;
    res = res &&
          vkenv_createImage(&memory->octave_image_arr[oct_idx], memory->device, 0, VK_IMAGE_TYPE_2D, pyramid_format,
                            (VkExtent3D){.width = width, .height = height, .depth = 1}, 1, memory->nb_scales_per_octave + 3, VK_SAMPLE_COUNT_1_BIT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED);

    vkGetImageMemoryRequirements(memory->device->device, memory->octave_image_arr[oct_idx], &memory_requirement);
    if (memory_requirement.size > memory->octave_image_memory_size_arr[oct_idx])
    {
      VK_NULL_SAFE_DELETE(memory->octave_image_memory_arr[oct_idx], vkFreeMemory(memory->device->device, memory->octave_image_memory_arr[oct_idx], NULL));
      res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
      res = res && vkenv_allocateMemory(&memory->octave_image_memory_arr[oct_idx], memory->device, memory_requirement.size, memory_type_idx);
      memory->octave_image_memory_size_arr[oct_idx] = memory_requirement.size;
      logInfo(LOG_TAG, "Octave image (oct %d) (%d,%d) realloc", oct_idx, width, height);
    }
    res = res && vkenv_bindImageMemory(memory->device, memory->octave_image_arr[oct_idx], memory->octave_image_memory_arr[oct_idx], 0);
    res = res && vkenv_createImageView(&memory->octave_image_view_arr[oct_idx], memory->device, 0, memory->octave_image_arr[oct_idx],
                                       VK_IMAGE_VIEW_TYPE_2D_ARRAY, pyramid_format, VKENV_DEFAULT_COMPONENT_MAPPING,
                                       (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, memory->nb_scales_per_octave + 3});
  }
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the octave images");
    return false;
  }

  // Create DoG image array per octave
  res = true;
  for (uint32_t oct_idx = 0; oct_idx < memory->max_nb_octaves; oct_idx++)
  {
    uint32_t width = memory->octave_resolutions[oct_idx].width;
    uint32_t height = memory->octave_resolutions[oct_idx].height;
    res = res &&
          vkenv_createImage(&memory->octave_DoG_image_arr[oct_idx], memory->device, 0, VK_IMAGE_TYPE_2D, pyramid_format,
                            (VkExtent3D){.width = width, .height = height, .depth = 1}, 1, memory->nb_scales_per_octave + 2, VK_SAMPLE_COUNT_1_BIT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED);

    vkGetImageMemoryRequirements(memory->device->device, memory->octave_DoG_image_arr[oct_idx], &memory_requirement);
    if (memory_requirement.size > memory->octave_DoG_image_memory_size_arr[oct_idx])
    {
      VK_NULL_SAFE_DELETE(memory->octave_DoG_image_memory_arr[oct_idx],
                          vkFreeMemory(memory->device->device, memory->octave_DoG_image_memory_arr[oct_idx], NULL));
      res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
      res = res && vkenv_allocateMemory(&memory->octave_DoG_image_memory_arr[oct_idx], memory->device, memory_requirement.size, memory_type_idx);
      memory->octave_DoG_image_memory_size_arr[oct_idx] = memory_requirement.size;
      logInfo(LOG_TAG, "Octave DoG image (oct %d) (%d,%d) realloc", oct_idx, width, height);
    }
    res = res && vkenv_bindImageMemory(memory->device, memory->octave_DoG_image_arr[oct_idx], memory->octave_DoG_image_memory_arr[oct_idx], 0);
    res = res && vkenv_createImageView(&memory->octave_DoG_image_view_arr[oct_idx], memory->device, 0, memory->octave_DoG_image_arr[oct_idx],
                                       VK_IMAGE_VIEW_TYPE_2D_ARRAY, pyramid_format, VKENV_DEFAULT_COMPONENT_MAPPING,
                                       (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, memory->nb_scales_per_octave + 2});
  }
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the DoG octave images");
    return false;
  }

  //////////////////////////////////////////////////////////////////////
  // Setup the image layouts
  //////////////////////////////////////////////////////////////////////
  res = true;
  VkCommandBuffer layout_change_cmdbuf = NULL;
  res = res && vkenv_beginInstantCommandBuffer(memory->device->device, memory->general_command_pool, &layout_change_cmdbuf);
  // Set the input image, blur temp images, octave images and DoG images to VK_LAYOUT_GENERAL
  VkImageMemoryBarrier *layout_change_barriers = (VkImageMemoryBarrier *)malloc(sizeof(VkImageMemoryBarrier) * (1 + (memory->max_nb_octaves * 3)));
  layout_change_barriers[0] =
      vkenv_genImageMemoryBarrier(memory->input_image, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  int layout_change_barrier_cnt = 1;

  for (uint32_t i = 0; i < memory->max_nb_octaves; i++)
  {
    layout_change_barriers[layout_change_barrier_cnt + 0] =
        vkenv_genImageMemoryBarrier(memory->blur_tmp_image_arr[i], 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    layout_change_barriers[layout_change_barrier_cnt + 1] = vkenv_genImageMemoryBarrier(
        memory->octave_image_arr[i], 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, memory->nb_scales_per_octave + 3});

    layout_change_barriers[layout_change_barrier_cnt + 2] = vkenv_genImageMemoryBarrier(
        memory->octave_DoG_image_arr[i], 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, memory->nb_scales_per_octave + 2});

    layout_change_barrier_cnt += 3;
  }
  vkCmdPipelineBarrier(layout_change_cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL,
                       layout_change_barrier_cnt, layout_change_barriers);

  free(layout_change_barriers);
  res = res && vkenv_endInstantCommandBuffer(memory->device->device, memory->device->general_queue, memory->general_command_pool, layout_change_cmdbuf);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the initial layout for the images");
    return false;
  }

  return true;
}

bool setupStaticObjectsAndMemory(vksift_SiftMemory memory)
{
  bool res;
  VkMemoryRequirements memory_requirement;
  uint32_t memory_type_idx;
  //////////////////////////////////////////////////////////////////////
  // Setup input image staging buffer and output image (they don't depend on the input resolution)
  //////////////////////////////////////////////////////////////////////

  // Create image staging buffer and memory for the input and output images of max size
  // The biggest output images will be the float32 scale image of the largest octave (potential upsampling)
  res = true;
  VkDeviceSize image_staging_size = 4 * (memory->octave_resolutions[0].width * memory->octave_resolutions[0].height);
  res = res && vkenv_createBuffer(&memory->image_staging_buffer, memory->device, 0, image_staging_size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->image_staging_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->image_staging_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindBufferMemory(memory->device, memory->image_staging_buffer, memory->image_staging_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the image staging buffer");
    return false;
  }

  // Create output image and image view
  res = true;
  res = res && vkenv_createImage(&memory->output_image, memory->device, 0, VK_IMAGE_TYPE_2D, VK_FORMAT_R32_SFLOAT,
                                 (VkExtent3D){.width = memory->octave_resolutions[0].width, .height = memory->octave_resolutions[0].height, .depth = 1}, 1,
                                 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE,
                                 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED);
  vkGetImageMemoryRequirements(memory->device->device, memory->output_image, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->output_image_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindImageMemory(memory->device, memory->output_image, memory->output_image_memory, 0u);
  // We destroy this image right away because it will be created on the allocated memory at runtime to match the specs of the image the user want to
  // retrieve. It must always be released after being used.
  VK_NULL_SAFE_DELETE(memory->output_image, vkDestroyImage(memory->device->device, memory->output_image, NULL));
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the output image");
    return false;
  }

  //////////////////////////////////////////////////////////////////////
  // Setup SIFT buffers related objects
  //////////////////////////////////////////////////////////////////////
  // Create the SIFT buffers
  res = true;
  for (uint32_t buff_idx = 0; buff_idx < memory->nb_sift_buffer; buff_idx++)
  {
    // For each section (when not packed), there's a header containing 2 uint32_t containing the current nb of SIFT found for the section
    // and the max number of SIFT that can be stored in this section.
    // When packed there a single uint32_t header containing the full number of SIFT features in the buffer
    VkDeviceSize buffer_size = (sizeof(uint32_t) * 2 * memory->max_nb_octaves) + (memory->max_nb_sift_per_buffer * sizeof(vksift_Feature));
    res = res && vkenv_createBuffer(&memory->sift_buffer_arr[buff_idx], memory->device, 0, buffer_size,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
    vkGetBufferMemoryRequirements(memory->device->device, memory->sift_buffer_arr[buff_idx], &memory_requirement);
    res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
    res = res && vkenv_allocateMemory(&memory->sift_buffer_memory_arr[buff_idx], memory->device, memory_requirement.size, memory_type_idx);
    res = res && vkenv_bindBufferMemory(memory->device, memory->sift_buffer_arr[buff_idx], memory->sift_buffer_memory_arr[buff_idx], 0u);
  }
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the SIFT buffers");
    return false;
  }

  // Create the SIFT count staging buffer
  res = true;
  VkDeviceSize info_buffer_size = sizeof(uint32_t) * memory->max_nb_octaves * memory->nb_sift_buffer;
  res = res && vkenv_createBuffer(&memory->sift_count_staging_buffer, memory->device, 0, info_buffer_size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->sift_count_staging_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->sift_count_staging_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindBufferMemory(memory->device, memory->sift_count_staging_buffer, memory->sift_count_staging_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the SIFT info buffer");
    return false;
  }

  // Create the SIFT staging buffer
  res = true;
  VkDeviceSize sift_staging_buffer_size = memory->max_nb_sift_per_buffer * sizeof(vksift_Feature);
  res = res && vkenv_createBuffer(&memory->sift_staging_buffer, memory->device, 0, sift_staging_buffer_size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->sift_staging_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->sift_staging_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindBufferMemory(memory->device, memory->sift_staging_buffer, memory->sift_staging_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the SIFT staging buffer");
    return false;
  }

  //////////////////////////////////////////////////////////////////////
  // Setup matching related objects
  //////////////////////////////////////////////////////////////////////
  // Create the match buffer
  res = true;
  res = res && vkenv_createBuffer(&memory->match_output_buffer, memory->device, 0, memory->max_nb_sift_per_buffer * sizeof(vksift_Match_2NN),
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->match_output_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->match_output_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindBufferMemory(memory->device, memory->match_output_buffer, memory->match_output_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the match result buffer");
    return false;
  }
  // Create the match staging buffer
  res = true;
  res = res && vkenv_createBuffer(&memory->match_output_staging_buffer, memory->device, 0, memory->max_nb_sift_per_buffer * sizeof(vksift_Match_2NN),
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->match_output_staging_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->match_output_staging_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindBufferMemory(memory->device, memory->match_output_staging_buffer, memory->match_output_staging_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the match result staging buffer");
    return false;
  }

  //////////////////////////////////////////////////////////////////////
  // Setup other objects
  //////////////////////////////////////////////////////////////////////
  // Create the orientation pipeline indirect dispatch buffer
  res = true;
  VkDeviceSize indirect_orientation_dispatch_buffer_size = (3 * sizeof(uint32_t)) * memory->max_nb_octaves;
  res = res && vkenv_createBuffer(&memory->indirect_orientation_dispatch_buffer, memory->device, 0, indirect_orientation_dispatch_buffer_size,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->indirect_orientation_dispatch_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->indirect_orientation_dispatch_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res =
      res && vkenv_bindBufferMemory(memory->device, memory->indirect_orientation_dispatch_buffer, memory->indirect_orientation_dispatch_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the indirect orientation dispatch buffer");
    return false;
  }

  // Create the descriptor pipeline indirect dispatch buffer
  res = true;
  VkDeviceSize indirect_descriptor_dispatch_buffer_size = (3 * sizeof(uint32_t)) * memory->max_nb_octaves;
  res = res && vkenv_createBuffer(&memory->indirect_descriptor_dispatch_buffer, memory->device, 0, indirect_descriptor_dispatch_buffer_size,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->indirect_descriptor_dispatch_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->indirect_descriptor_dispatch_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindBufferMemory(memory->device, memory->indirect_descriptor_dispatch_buffer, memory->indirect_descriptor_dispatch_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the indirect descriptor dispatch buffer");
    return false;
  }

  // Create the matcher pipeline indirect dispatch buffer
  res = true;
  VkDeviceSize indirect_matcher_dispatch_buffer_size = 3 * sizeof(uint32_t);
  res = res && vkenv_createBuffer(&memory->indirect_matcher_dispatch_buffer, memory->device, 0, indirect_matcher_dispatch_buffer_size,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_SHARING_MODE_EXCLUSIVE, 0, NULL);
  vkGetBufferMemoryRequirements(memory->device->device, memory->indirect_matcher_dispatch_buffer, &memory_requirement);
  res = res && vkenv_findValidMemoryType(memory->device->physical_device, memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_idx);
  res = res && vkenv_allocateMemory(&memory->indirect_matcher_dispatch_buffer_memory, memory->device, memory_requirement.size, memory_type_idx);
  res = res && vkenv_bindBufferMemory(memory->device, memory->indirect_matcher_dispatch_buffer, memory->indirect_matcher_dispatch_buffer_memory, 0u);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when setting up the indirect matcher dispatch buffer");
    return false;
  }

  //////////////////////////////////////////////////////////////////////
  // Map staging objects
  //////////////////////////////////////////////////////////////////////
  res = true;
  res = res &&
        (vkMapMemory(memory->device->device, memory->image_staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &memory->image_staging_buffer_ptr) == VK_SUCCESS);
  res = res &&
        (vkMapMemory(memory->device->device, memory->sift_staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &memory->sift_staging_buffer_ptr) == VK_SUCCESS);
  res = res && (vkMapMemory(memory->device->device, memory->sift_count_staging_buffer_memory, 0, VK_WHOLE_SIZE, 0,
                            &memory->sift_count_staging_buffer_ptr) == VK_SUCCESS);
  res = res && (vkMapMemory(memory->device->device, memory->match_output_staging_buffer_memory, 0, VK_WHOLE_SIZE, 0,
                            &memory->match_output_staging_buffer_ptr) == VK_SUCCESS);
  if (!res)
  {
    logError(LOG_TAG, "An error occured when mapping the staging buffers");
    return false;
  }

  return true;
}

bool vksift_createSiftMemory(vkenv_Device device, vksift_SiftMemory *memory_ptr, const vksift_Config *config)
{
  assert(memory_ptr != NULL);
  assert(config != NULL);
  assert(*memory_ptr == NULL);

  *memory_ptr = (vksift_SiftMemory)malloc(sizeof(struct vksift_SiftMemory_T));
  vksift_SiftMemory memory = *memory_ptr;
  memset(memory, 0, sizeof(struct vksift_SiftMemory_T));

  // Copy parent device (used for almost every vulkan call)
  memory->device = device;
  // Copy configuration info
  memory->max_image_size = config->input_image_max_size;
  memory->nb_scales_per_octave = config->nb_scales_per_octave;
  memory->nb_sift_buffer = config->sift_buffer_count;
  memory->max_nb_sift_per_buffer = config->max_nb_sift_per_buffer;
  memory->pyr_precision_mode = config->pyramid_precision_mode;
  memory->use_upsampling = config->use_input_upsampling;

  // Define default input image width/height from configuration
  memory->curr_input_image_width = ceilf(sqrtf((float)memory->max_image_size));
  memory->curr_input_image_height = memory->curr_input_image_width;
  // Update max size to account for float rounding in the default width/height
  memory->max_image_size = memory->curr_input_image_width * memory->curr_input_image_height;

  // Compute or set the maximum number of octaves for the largest possible image size.
  // We want the lowest dimension of smallest octave image resolution to be more than 16 pixels.
  // Since here the default width is the same as the height and width*height is the maximal image size there will never be a
  // lowest image dimension superior to the current width/height, so the maximum number of octave we will ever have is the number of
  // successive x2 downsampling + 1 (+1 is for the octave with input image resolution) such that the downscalled width/height is more than 16 pixels.
  // Width/height being coded by log2(width) bits, we can perform exactly log2(width)-log(16) = log2(width)-4 successing x2 subsampling.
  memory->max_nb_octaves = log2f((float)memory->curr_input_image_width) - 4 + (memory->use_upsampling ? 2 : 1);
  // If user defined the number of octave per image take the minimum number of octave
  if (config->nb_octaves > 0 && config->nb_octaves < memory->max_nb_octaves)
  {
    memory->max_nb_octaves = config->nb_octaves;
  }

  // Allocate octave resolution array [oct0_width, oct0_height, oct1_width, ...]
  memory->octave_resolutions = (vksift_OctaveResolution *)malloc(sizeof(vksift_OctaveResolution) * memory->max_nb_octaves);
  // Update octave resolutions info for the default input resolution
  updateScaleSpaceInfo(memory);

  // Allocate SIFT buffers structure info array
  memory->sift_buffers_info = (vksift_SiftBufferInfo *)malloc(sizeof(vksift_SiftBufferInfo) * memory->nb_sift_buffer);
  memset(memory->sift_buffers_info, 0, sizeof(vksift_SiftBufferInfo) * memory->nb_sift_buffer);
  for (uint32_t i = 0; i < memory->nb_sift_buffer; i++)
  {
    memory->sift_buffers_info[i].octave_section_max_nb_feat_arr = (uint32_t *)malloc(sizeof(uint32_t) * memory->max_nb_octaves);
    memset(memory->sift_buffers_info[i].octave_section_max_nb_feat_arr, 0, sizeof(uint32_t) * memory->max_nb_octaves);
    updateBufferInfo(memory, i);
  }

  // Allocate Vulkan object lists (and set to NULL)
  memory->sift_buffer_arr = (VkBuffer *)malloc(sizeof(VkBuffer) * memory->nb_sift_buffer);
  memory->sift_buffer_memory_arr = (VkDeviceMemory *)malloc(sizeof(VkDeviceMemory) * memory->nb_sift_buffer);
  memset(memory->sift_buffer_arr, 0, sizeof(VkBuffer) * memory->nb_sift_buffer);
  memset(memory->sift_buffer_memory_arr, 0, sizeof(VkDeviceMemory) * memory->nb_sift_buffer);

  memory->blur_tmp_image_arr = (VkImage *)malloc(sizeof(VkImage) * memory->max_nb_octaves);
  memory->blur_tmp_image_view_arr = (VkImageView *)malloc(sizeof(VkImageView) * memory->max_nb_octaves);
  memory->blur_tmp_image_memory_arr = (VkDeviceMemory *)malloc(sizeof(VkDeviceMemory) * memory->max_nb_octaves);
  memory->blur_tmp_image_memory_size_arr = (VkDeviceSize *)malloc(sizeof(VkDeviceSize) * memory->max_nb_octaves);
  memset(memory->blur_tmp_image_arr, 0, sizeof(VkImage) * memory->max_nb_octaves);
  memset(memory->blur_tmp_image_view_arr, 0, sizeof(VkImageView) * memory->max_nb_octaves);
  memset(memory->blur_tmp_image_memory_arr, 0, sizeof(VkDeviceMemory) * memory->max_nb_octaves);
  memset(memory->blur_tmp_image_memory_size_arr, 0, sizeof(VkDeviceSize) * memory->max_nb_octaves);

  memory->octave_image_arr = (VkImage *)malloc(sizeof(VkImage) * memory->max_nb_octaves);
  memory->octave_image_view_arr = (VkImageView *)malloc(sizeof(VkImageView) * memory->max_nb_octaves);
  memory->octave_image_memory_arr = (VkDeviceMemory *)malloc(sizeof(VkDeviceMemory) * memory->max_nb_octaves);
  memory->octave_image_memory_size_arr = (VkDeviceSize *)malloc(sizeof(VkDeviceSize) * memory->max_nb_octaves);
  memset(memory->octave_image_arr, 0, sizeof(VkImage) * memory->max_nb_octaves);
  memset(memory->octave_image_view_arr, 0, sizeof(VkImageView) * memory->max_nb_octaves);
  memset(memory->octave_image_memory_arr, 0, sizeof(VkDeviceMemory) * memory->max_nb_octaves);
  memset(memory->octave_image_memory_size_arr, 0, sizeof(VkDeviceSize) * memory->max_nb_octaves);

  memory->octave_DoG_image_arr = (VkImage *)malloc(sizeof(VkImage) * memory->max_nb_octaves);
  memory->octave_DoG_image_view_arr = (VkImageView *)malloc(sizeof(VkImageView) * memory->max_nb_octaves);
  memory->octave_DoG_image_memory_arr = (VkDeviceMemory *)malloc(sizeof(VkDeviceMemory) * memory->max_nb_octaves);
  memory->octave_DoG_image_memory_size_arr = (VkDeviceSize *)malloc(sizeof(VkDeviceSize) * memory->max_nb_octaves);
  memset(memory->octave_DoG_image_arr, 0, sizeof(VkImage) * memory->max_nb_octaves);
  memset(memory->octave_DoG_image_view_arr, 0, sizeof(VkImageView) * memory->max_nb_octaves);
  memset(memory->octave_DoG_image_memory_arr, 0, sizeof(VkDeviceMemory) * memory->max_nb_octaves);
  memset(memory->octave_DoG_image_memory_size_arr, 0, sizeof(VkDeviceSize) * memory->max_nb_octaves);

  // Setup the command pools
  VkCommandPoolCreateInfo general_cmdpool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                  .pNext = NULL,
                                                  .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                  .queueFamilyIndex = device->general_queue_family_idx};
  if (vkCreateCommandPool(memory->device->device, &general_cmdpool_info, NULL, &memory->general_command_pool) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Sift memory creation failed: could not setup the general purpose command pool");
    vksift_destroySiftMemory(memory_ptr);
    return false;
  }

  // Setup the Vulkan objects
  if (!setupStaticObjectsAndMemory(memory) || !setupDynamicObjectsAndMemory(memory))
  {
    logError(LOG_TAG, "Failed to create the SiftMemory instance");
    vksift_destroySiftMemory(memory_ptr);
    return false;
  }

  return true;
}

void vksift_destroySiftMemory(vksift_SiftMemory *memory_ptr)
{
  assert(memory_ptr != NULL);
  assert(*memory_ptr != NULL); // vksift_destroyMemory shouldn't be called on NULL Memory object
  vksift_SiftMemory memory = *memory_ptr;

  // Unmap any persistently mapped memory allocation
  if (memory->image_staging_buffer_memory != NULL)
  {
    vkUnmapMemory(memory->device->device, memory->image_staging_buffer_memory);
  }
  if (memory->sift_staging_buffer_memory != NULL)
  {
    vkUnmapMemory(memory->device->device, memory->sift_staging_buffer_memory);
  }
  if (memory->sift_count_staging_buffer_memory != NULL)
  {
    vkUnmapMemory(memory->device->device, memory->sift_count_staging_buffer_memory);
  }
  if (memory->match_output_staging_buffer_memory != NULL)
  {
    vkUnmapMemory(memory->device->device, memory->match_output_staging_buffer_memory);
  }

  // Destroy Vulkan buffers and memory
  for (uint32_t i = 0; i < memory->nb_sift_buffer; i++)
  {
    VK_NULL_SAFE_DELETE(memory->sift_buffer_arr[i], vkDestroyBuffer(memory->device->device, memory->sift_buffer_arr[i], NULL));
    VK_NULL_SAFE_DELETE(memory->sift_buffer_memory_arr[i], vkFreeMemory(memory->device->device, memory->sift_buffer_memory_arr[i], NULL));
  }
  VK_NULL_SAFE_DELETE(memory->sift_count_staging_buffer, vkDestroyBuffer(memory->device->device, memory->sift_count_staging_buffer, NULL));
  VK_NULL_SAFE_DELETE(memory->sift_staging_buffer, vkDestroyBuffer(memory->device->device, memory->sift_staging_buffer, NULL));
  VK_NULL_SAFE_DELETE(memory->image_staging_buffer, vkDestroyBuffer(memory->device->device, memory->image_staging_buffer, NULL));
  VK_NULL_SAFE_DELETE(memory->match_output_buffer, vkDestroyBuffer(memory->device->device, memory->match_output_buffer, NULL));
  VK_NULL_SAFE_DELETE(memory->match_output_staging_buffer, vkDestroyBuffer(memory->device->device, memory->match_output_staging_buffer, NULL));
  VK_NULL_SAFE_DELETE(memory->indirect_orientation_dispatch_buffer,
                      vkDestroyBuffer(memory->device->device, memory->indirect_orientation_dispatch_buffer, NULL));
  VK_NULL_SAFE_DELETE(memory->indirect_descriptor_dispatch_buffer,
                      vkDestroyBuffer(memory->device->device, memory->indirect_descriptor_dispatch_buffer, NULL));
  VK_NULL_SAFE_DELETE(memory->indirect_matcher_dispatch_buffer, vkDestroyBuffer(memory->device->device, memory->indirect_matcher_dispatch_buffer, NULL));

  VK_NULL_SAFE_DELETE(memory->sift_count_staging_buffer_memory, vkFreeMemory(memory->device->device, memory->sift_count_staging_buffer_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->sift_staging_buffer_memory, vkFreeMemory(memory->device->device, memory->sift_staging_buffer_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->image_staging_buffer_memory, vkFreeMemory(memory->device->device, memory->image_staging_buffer_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->match_output_buffer_memory, vkFreeMemory(memory->device->device, memory->match_output_buffer_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->match_output_staging_buffer_memory, vkFreeMemory(memory->device->device, memory->match_output_staging_buffer_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->indirect_orientation_dispatch_buffer_memory,
                      vkFreeMemory(memory->device->device, memory->indirect_orientation_dispatch_buffer_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->indirect_descriptor_dispatch_buffer_memory,
                      vkFreeMemory(memory->device->device, memory->indirect_descriptor_dispatch_buffer_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->indirect_matcher_dispatch_buffer_memory,
                      vkFreeMemory(memory->device->device, memory->indirect_matcher_dispatch_buffer_memory, NULL));

  // Destroy Vulkan images and memory
  for (uint32_t i = 0; i < memory->max_nb_octaves; i++)
  {
    VK_NULL_SAFE_DELETE(memory->blur_tmp_image_view_arr[i], vkDestroyImageView(memory->device->device, memory->blur_tmp_image_view_arr[i], NULL));
    VK_NULL_SAFE_DELETE(memory->octave_image_view_arr[i], vkDestroyImageView(memory->device->device, memory->octave_image_view_arr[i], NULL));
    VK_NULL_SAFE_DELETE(memory->octave_DoG_image_view_arr[i], vkDestroyImageView(memory->device->device, memory->octave_DoG_image_view_arr[i], NULL));

    VK_NULL_SAFE_DELETE(memory->blur_tmp_image_arr[i], vkDestroyImage(memory->device->device, memory->blur_tmp_image_arr[i], NULL));
    VK_NULL_SAFE_DELETE(memory->octave_image_arr[i], vkDestroyImage(memory->device->device, memory->octave_image_arr[i], NULL));
    VK_NULL_SAFE_DELETE(memory->octave_DoG_image_arr[i], vkDestroyImage(memory->device->device, memory->octave_DoG_image_arr[i], NULL));

    VK_NULL_SAFE_DELETE(memory->blur_tmp_image_memory_arr[i], vkFreeMemory(memory->device->device, memory->blur_tmp_image_memory_arr[i], NULL));
    VK_NULL_SAFE_DELETE(memory->octave_image_memory_arr[i], vkFreeMemory(memory->device->device, memory->octave_image_memory_arr[i], NULL));
    VK_NULL_SAFE_DELETE(memory->octave_DoG_image_memory_arr[i], vkFreeMemory(memory->device->device, memory->octave_DoG_image_memory_arr[i], NULL));
  }
  VK_NULL_SAFE_DELETE(memory->input_image_view, vkDestroyImageView(memory->device->device, memory->input_image_view, NULL));
  VK_NULL_SAFE_DELETE(memory->input_image, vkDestroyImage(memory->device->device, memory->input_image, NULL));
  VK_NULL_SAFE_DELETE(memory->output_image, vkDestroyImage(memory->device->device, memory->output_image, NULL));
  VK_NULL_SAFE_DELETE(memory->input_image_memory, vkFreeMemory(memory->device->device, memory->input_image_memory, NULL));
  VK_NULL_SAFE_DELETE(memory->output_image_memory, vkFreeMemory(memory->device->device, memory->output_image_memory, NULL));

  // Destroy command pool
  VK_NULL_SAFE_DELETE(memory->general_command_pool, vkDestroyCommandPool(memory->device->device, memory->general_command_pool, NULL));

  // Release any allocated data
  free(memory->sift_buffer_arr);
  free(memory->sift_buffer_memory_arr);
  free(memory->blur_tmp_image_arr);
  free(memory->blur_tmp_image_view_arr);
  free(memory->blur_tmp_image_memory_arr);
  free(memory->octave_image_arr);
  free(memory->octave_image_view_arr);
  free(memory->octave_image_memory_arr);
  free(memory->octave_DoG_image_arr);
  free(memory->octave_DoG_image_view_arr);
  free(memory->octave_DoG_image_memory_arr);
  for (uint32_t i = 0; i < memory->nb_sift_buffer; i++)
  {
    free(memory->sift_buffers_info[i].octave_section_max_nb_feat_arr);
  }
  free(memory->sift_buffers_info);
  free(memory->octave_resolutions);

  // Releave vksift_Memory memory
  free(*memory_ptr);
  *memory_ptr = NULL;
}

bool vksift_prepareSiftMemoryForInput(vksift_SiftMemory memory, const uint8_t *image_data, const uint32_t input_width, const uint32_t input_height,
                                      const uint32_t target_buffer_idx, bool *memory_layout_updated)
{
  if (memory->curr_input_image_width != input_width || memory->curr_input_image_height != input_height)
  {
    // If the input resolution change, we need to recreate the image/views to fit the new pyramid size
    memory->curr_input_image_width = input_width;
    memory->curr_input_image_height = input_height;
    updateScaleSpaceInfo(memory); // update scalespace resolutions and define the number of octave
    // Destroy pyramid related buffers, images and views and try to recreate them on the memory allocated for the max input size
    // (to avoid extremely slow memory reallocation)
    VK_NULL_SAFE_DELETE(memory->input_image_view, vkDestroyImageView(memory->device->device, memory->input_image_view, NULL));
    VK_NULL_SAFE_DELETE(memory->input_image, vkDestroyImage(memory->device->device, memory->input_image, NULL));
    for (uint32_t oct_idx = 0; oct_idx < memory->max_nb_octaves; oct_idx++)
    {
      VK_NULL_SAFE_DELETE(memory->blur_tmp_image_view_arr[oct_idx],
                          vkDestroyImageView(memory->device->device, memory->blur_tmp_image_view_arr[oct_idx], NULL));
      VK_NULL_SAFE_DELETE(memory->blur_tmp_image_arr[oct_idx], vkDestroyImage(memory->device->device, memory->blur_tmp_image_arr[oct_idx], NULL));
      VK_NULL_SAFE_DELETE(memory->octave_image_view_arr[oct_idx],
                          vkDestroyImageView(memory->device->device, memory->octave_image_view_arr[oct_idx], NULL));
      VK_NULL_SAFE_DELETE(memory->octave_image_arr[oct_idx], vkDestroyImage(memory->device->device, memory->octave_image_arr[oct_idx], NULL));
      VK_NULL_SAFE_DELETE(memory->octave_DoG_image_view_arr[oct_idx],
                          vkDestroyImageView(memory->device->device, memory->octave_DoG_image_view_arr[oct_idx], NULL));
      VK_NULL_SAFE_DELETE(memory->octave_DoG_image_arr[oct_idx], vkDestroyImage(memory->device->device, memory->octave_DoG_image_arr[oct_idx], NULL));
    }
    // Recreate objects
    if (!setupDynamicObjectsAndMemory(memory))
    {
      logError(LOG_TAG, "Failed to update the Vulkan images for the new input resolution");
      return false;
    }

    *memory_layout_updated = true;
  }

  if (memory->sift_buffers_info[target_buffer_idx].curr_input_width != memory->curr_input_image_width ||
      memory->sift_buffers_info[target_buffer_idx].curr_input_height != memory->curr_input_image_height)
  {
    // If the buffer wasn't using this resolution before we must also update its sections information according to the current pyramid
    updateBufferInfo(memory, target_buffer_idx);
    *memory_layout_updated = true;
  }

  // Copy input image to staging buffer
  VkMappedMemoryRange memory_range = {
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .pNext = NULL, .memory = memory->image_staging_buffer_memory, .offset = 0, .size = VK_WHOLE_SIZE};
  if (vkInvalidateMappedMemoryRanges(memory->device->device, 1, &memory_range) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to invalidate staging mapped memory when copying new image data");
    return false;
  }

  memcpy(memory->image_staging_buffer_ptr, image_data, sizeof(uint8_t) * input_width * input_height);

  if (vkFlushMappedMemoryRanges(memory->device->device, 1, &memory_range) != VK_SUCCESS)
  {
    logError(LOG_TAG, "Failed to flush staging mapped memory when copying new image data");
    return false;
  }

  return true;
}
