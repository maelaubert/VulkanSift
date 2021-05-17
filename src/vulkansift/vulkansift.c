#include "vulkansift/vulkansift.h"

// vkenv
#include "vulkansift/vkenv/logger.h"
#include "vulkansift/vkenv/vulkan_device.h"
#include "vulkansift/vkenv/vulkan_utils.h"
#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
#include "vulkansift/vkenv/debug_presenter.h"
#include "vulkansift/vkenv/mini_window.h"
#endif

// vksift
#include "vulkansift/sift_detector.h"
#include "vulkansift/sift_matcher.h"
#include "vulkansift/sift_memory.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char LOG_TAG[] = "VulkanSift";

bool vksift_loadVulkan()
{
  vkenv_InstanceConfig instance_config = {.application_name = "VulkanSift",
                                          .application_version = VK_MAKE_VERSION(1, 0, 0),
                                          .engine_name = "",
                                          .engine_version = VK_MAKE_VERSION(1, 0, 0),
                                          .validation_layer_count = 0,
                                          .validation_layers = NULL,
                                          .instance_extension_count = 0,
                                          .instance_extensions = NULL};

#ifndef NDEBUG
  // Activate Vulkan debug layers on debug mode only
  const char *validation_layer_name = "VK_LAYER_KHRONOS_validation";
  instance_config.validation_layer_count = 1;
  instance_config.validation_layers = &validation_layer_name;
#ifdef VKSIFT_GPU_DEBUG
  // If compiled with GPU debug support, add the required rendering extensions and the debug marker extension
  const char *instance_extensions[3];
  instance_extensions[0] = (const char *)VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  instance_extensions[1] = (const char *)VK_KHR_SURFACE_EXTENSION_NAME;
  instance_extensions[2] = vkenv_getMiniWindowSurfaceExtensionName();
  instance_config.instance_extension_count = 3;
  instance_config.instance_extensions = instance_extensions;
#endif
#endif

  return vkenv_createInstance(&instance_config);
}

void vksift_unloadVulkan() { vkenv_destroyInstance(); }

void vksift_getAvailableGPUs(uint32_t *gpu_count, char *gpu_names[256])
{
  if (gpu_names == NULL)
  {
    vkenv_getPhysicalDevicesProperties(gpu_count, NULL);
  }
  else
  {
    VkPhysicalDeviceProperties *devices_props = (VkPhysicalDeviceProperties *)malloc(sizeof(VkPhysicalDeviceProperties) * (*gpu_count));
    vkenv_getPhysicalDevicesProperties(gpu_count, devices_props);
    for (uint32_t i = 0; i < *gpu_count; i++)
    {
      memcpy(gpu_names[i], devices_props[i].deviceName, 256);
    }
    free(devices_props);
  }
}

void vksift_setLogLevel(vksift_LogLevel level)
{
  switch (level)
  {
  case VKSIFT_NO_LOG:
    vkenv_setLogLevel(VKENV_LOG_NONE);
    break;
  case VKSIFT_LOG_ERROR:
    vkenv_setLogLevel(VKENV_LOG_ERROR);
    break;
  case VKSIFT_LOG_WARNING:
    vkenv_setLogLevel(VKENV_LOG_WARNING);
    break;
  case VKSIFT_LOG_INFO:
    vkenv_setLogLevel(VKENV_LOG_INFO);
    break;
  case VKSIFT_LOG_DEBUG:
    vkenv_setLogLevel(VKENV_LOG_DEBUG);
    break;
  default:
    logError(LOG_TAG, "vksift_LogLevel in vksift_setLogLevel() is not handled");
    break;
  }
}

typedef struct vksift_Instance_T
{
  vkenv_Device vulkan_device;
  vksift_SiftMemory sift_memory;
  vksift_SiftDetector sift_detector;
  vksift_SiftMatcher sift_matcher;

#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
  vkenv_DebugPresenter debug_presenter;
#endif
} vksift_Instance_T;

bool checkConfigCond(bool cond, const char *msg_on_cond_false)
{
  if (!cond)
  {
    logError(LOG_TAG, msg_on_cond_false);
  }
  return cond;
}

bool isConfigurationValid(const vksift_Config *config)
{
  // Check that the gaussian kernel sigma for the scale-space seed image if superior or equals to 0.
  bool valid_seed_gaussian_kernel = ((config->use_input_upsampling ? 2.f : 1.f) * config->input_image_blur_level) <= config->seed_scale_sigma;

  bool config_valid = true;
  config_valid &= checkConfigCond(config->input_image_max_size > 0, "Invalid configuration: input image size must be more than zero");
  config_valid &= checkConfigCond(config->sift_buffer_count > 0, "Invalid configuration: number of SIFT buffers must be more than zero");
  config_valid &= checkConfigCond(config->max_nb_sift_per_buffer > 0, "Invalid configuration: number of SIFT features per buffers must be more than zero");
  config_valid &= checkConfigCond(config->nb_scales_per_octave > 0, "Invalid configuration: number of scales per octave must be more than zero");
  config_valid &= checkConfigCond(config->input_image_blur_level >= 0.f, "Invalid configuration: input image blur level cannot be negative");
  config_valid &= checkConfigCond(config->seed_scale_sigma >= 0, "Invalid configuration: seed scale blur level cannot be negative");
  config_valid &=
      checkConfigCond(valid_seed_gaussian_kernel,
                      "Invalid configuration: the input image blur level (2x if upscaling activated) must be less than the seed scale blur level");
  config_valid &= checkConfigCond(config->intensity_threshold >= 0.f, "Invalid configuration: the DoG intensity threshold cannot be negative");
  config_valid &= checkConfigCond(config->edge_threshold >= 0.f, "Invalid configuration: the DoG edge threshold cannot be negative");
  config_valid &= checkConfigCond(config->edge_threshold >= 0.f, "Invalid configuration: the DoG edge threshold cannot be negative");

  switch (config->pyramid_precision_mode)
  {
  case VKSIFT_PYRAMID_PRECISION_FLOAT16:
    break;
  case VKSIFT_PYRAMID_PRECISION_FLOAT32:
    break;
  default:
    logError(LOG_TAG, "Invalid configuration: invalid scale-space pyramid format precision specified)");
    config_valid &= false;
    break;
  }

  return config_valid;
}

bool isBufferIdxValid(vksift_Instance instance, const uint32_t buffer_idx)
{
  if (buffer_idx > instance->sift_memory->nb_sift_buffer)
  {
    logError(LOG_TAG, "Provided target buffer index is (%d) but the number of reserved buffers is (%d).", buffer_idx,
             instance->sift_memory->nb_sift_buffer);
    return false;
  }
  else
  {
    return true;
  }
}

bool isInputResolutionValid(vksift_Instance instance, const uint32_t input_width, const uint32_t input_height)
{
  uint32_t input_size = input_width * input_height;
  if (input_size > instance->sift_memory->max_image_size)
  {
    logError(LOG_TAG, "Provided input image size (%d*%d=%d) is more than the configured maximum image size (%d).", input_width, input_height, input_size,
             instance->sift_memory->max_image_size);
    return false;
  }
  else
  {
    return true;
  }
}

bool isInputFeatureCoundValid(vksift_Instance instance, const uint32_t nb_feats)
{
  if (nb_feats > instance->sift_memory->max_nb_sift_per_buffer)
  {
    logError(LOG_TAG, "Provided features count (%d) is more than the configured maximum number of features per GPU buffer size (%d).", nb_feats,
             instance->sift_memory->max_nb_sift_per_buffer);
    return false;
  }
  else
  {
    return true;
  }
}

bool isInputOctaveIdxValid(vksift_Instance instance, const uint32_t octave_idx)
{
  if (octave_idx >= instance->sift_memory->curr_nb_octaves)
  {
    logError(LOG_TAG, "Requested octave idx is %d but the current number of octaves is %d", octave_idx, instance->sift_memory->curr_nb_octaves);
    return false;
  }
  else
  {
    return true;
  }
}

bool isInputScaleIdxValid(vksift_Instance instance, const uint32_t scale_idx, bool is_dog)
{
  uint32_t add_scale = is_dog ? 2 : 3;
  if (scale_idx >= (instance->sift_memory->nb_scales_per_octave + add_scale))
  {
    logError(LOG_TAG, "Requested scale idx is %d but the number of %s scales is %d", scale_idx, is_dog ? "DoG" : "blurred",
             (instance->sift_memory->nb_scales_per_octave + add_scale));
    return false;
  }
  else
  {
    return true;
  }
}

bool vksift_createInstance(vksift_Instance *instance_ptr, const vksift_Config *config)
{
  assert(instance_ptr != NULL);
  assert(config != NULL);
  assert(*instance_ptr == NULL);

  // Check configuration validity
  if (!isConfigurationValid(config))
  {
    logError(LOG_TAG, "vksift_createInstance() failure: Invalid configuration detected.");
    return false;
  }

  *instance_ptr = (vksift_Instance)malloc(sizeof(vksift_Instance_T));
  vksift_Instance instance = *instance_ptr;
  memset(instance, 0, sizeof(vksift_Instance_T));

  // Setup device
  // We need two async transfer queue to properly do async transfers, one is only used by the memory for GPU download/upload, the other is for
  // detection/matching SIFT buffer ownership transfers
  vkenv_DeviceConfig gpu_config = {.device_extension_count = 0,
                                   .device_extensions = NULL,
                                   .nb_general_queues = 1,
                                   .nb_async_compute_queues = 0,
                                   .nb_async_transfer_queues = 2,
                                   .target_device_idx = config->gpu_device_index};
#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
  const char *device_extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  gpu_config.device_extension_count = 1;
  gpu_config.device_extensions = &device_extension_name;
#endif

  if (!vkenv_createDevice(&instance->vulkan_device, &gpu_config))
  {
    logError(LOG_TAG, "vksift_createInstance() failure: An error occured when creating the Vulkan device");
    vksift_destroyInstance(instance_ptr);
    return false;
  }

#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
  if (!vkenv_createDebugPresenter(instance->vulkan_device, &instance->debug_presenter, "VulkanSIFT debug"))
  {
    logError(LOG_TAG, "vksift_createInstance() failure: An error occured when creating a DebugPresenter instance");
    vksift_destroyInstance(instance_ptr);
    return false;
  }
#endif

  if (!vksift_createSiftMemory(instance->vulkan_device, &instance->sift_memory, config))
  {
    logError(LOG_TAG, "vksift_createInstance() failure: Failed to setup the required memory objects");
    vksift_destroyInstance(instance_ptr);
    return false;
  }

  if (!vksift_createSiftDetector(instance->vulkan_device, instance->sift_memory, &instance->sift_detector, config))
  {
    logError(LOG_TAG, "vksift_createInstance() failure: Failed to setup the SIFT detector");
    vksift_destroyInstance(instance_ptr);
    return false;
  }

  if (!vksift_createSiftMatcher(instance->vulkan_device, instance->sift_memory, &instance->sift_matcher, config))
  {
    logError(LOG_TAG, "vksift_createInstance() failure: Failed to setup the SIFT matcher");
    vksift_destroyInstance(instance_ptr);
    return false;
  }

  // Setup Matcher (descriptors, pipelines, cmdbufs)
  return true;
}

void vksift_destroyInstance(vksift_Instance *instance_ptr)
{
  assert(instance_ptr != NULL);
  assert(*instance_ptr != NULL); // vksift_destroyInstance shouldn't be called on NULL Instance
  vksift_Instance instance = *instance_ptr;

  if (instance->vulkan_device != NULL)
  {
    // Wait for anything running on the GPU to finish
    vkDeviceWaitIdle(instance->vulkan_device->device);
  }

  // Destroy SiftMatcher
  VK_NULL_SAFE_DELETE(instance->sift_matcher, vksift_destroySiftMatcher(&instance->sift_matcher));

  // Destroy SiftDetector
  VK_NULL_SAFE_DELETE(instance->sift_detector, vksift_destroySiftDetector(&instance->sift_detector));

  // Destroy SiftMemory
  VK_NULL_SAFE_DELETE(instance->sift_memory, vksift_destroySiftMemory(&instance->sift_memory));

#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
  // Destroy DebugPresenter
  VK_NULL_SAFE_DELETE(instance->debug_presenter, vkenv_destroyDebugPresenter(instance->vulkan_device, &instance->debug_presenter));
#endif

  // Destroy Vulkan device
  VK_NULL_SAFE_DELETE(instance->vulkan_device, vkenv_destroyDevice(&instance->vulkan_device));

  // Releave vksift_Instance memory
  free(*instance_ptr);
  *instance_ptr = NULL;
}

bool vksift_presentDebugFrame(vksift_Instance instance)
{
#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)

  return vkenv_presentDebugFrame(instance->vulkan_device, instance->debug_presenter);
#else
  logWarning(LOG_TAG, "vksift_presentDebugFrame() was called but library was not built with GPU DEBUG capabilities.");
  return false;
#endif
}

void vksift_detectFeatures(vksift_Instance instance, const uint8_t *image_data, const uint32_t image_width, const uint32_t image_height,
                           const uint32_t gpu_buffer_id)
{
  if (!isBufferIdxValid(instance, gpu_buffer_id) || !isInputResolutionValid(instance, image_width, image_height))
  {
    logError(LOG_TAG, "vksift_detectFeatures() error: invalid input.");
    abort();
  }

  // If a GPU task is currently using the buffer, wait for it to be available
  // If a detection is currently running, wait for it to end
  VkFence fences[2] = {instance->sift_memory->sift_buffer_fence_arr[gpu_buffer_id], instance->sift_detector->end_of_detection_fence};
  vkWaitForFences(instance->vulkan_device->device, 2, fences, VK_TRUE, UINT64_MAX);

  // Prepare memory for input resolution and update target buffer structure
  bool memory_layout_updated = false;
  if (!vksift_prepareSiftMemoryForDetection(instance->sift_memory, image_data, image_width, image_height, gpu_buffer_id, &memory_layout_updated))
  {
    logError(LOG_TAG, "vksift_detectFeatures() error: Failed to prepare the SiftMemory instance for the input image and target buffer");
    abort();
  }

  // Run the sift detector algorithm
  vksift_dispatchSiftDetection(instance->sift_detector, gpu_buffer_id, memory_layout_updated);
}

uint32_t vksift_getFeaturesNumber(vksift_Instance instance, const uint32_t gpu_buffer_id)
{
  if (!isBufferIdxValid(instance, gpu_buffer_id))
  {
    logError(LOG_TAG, "vksift_getFeaturesNumber() error: invalid input.");
    abort();
  }

  // If a GPU task is currently using the buffer, wait for it to be available
  VkFence fences[1] = {instance->sift_memory->sift_buffer_fence_arr[gpu_buffer_id]};
  vkWaitForFences(instance->vulkan_device->device, 1, fences, VK_TRUE, UINT64_MAX);

  uint32_t feat_count = 0;
  if (!vksift_Memory_getBufferFeatureCount(instance->sift_memory, gpu_buffer_id, &feat_count))
  {
    logError(LOG_TAG, "vksift_getFeaturesNumber() error when retrieving the number of detected SIFT features.");
    abort();
  }
  return feat_count;
}
void vksift_downloadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, uint32_t gpu_buffer_id)
{
  if (!isBufferIdxValid(instance, gpu_buffer_id))
  {
    logError(LOG_TAG, "vksift_downloadFeatures() error: invalid input.");
    abort();
  }

  // If a GPU task is currently using the buffer, wait for it to be available
  VkFence fences[1] = {instance->sift_memory->sift_buffer_fence_arr[gpu_buffer_id]};
  vkWaitForFences(instance->vulkan_device->device, 1, fences, VK_TRUE, UINT64_MAX);

  if (!vksift_Memory_copyBufferFeaturesFromGPU(instance->sift_memory, gpu_buffer_id, feats_ptr))
  {
    logError(LOG_TAG, "vksift_downloadFeatures() error when downloading detection results.");
    abort();
  }
}

void vksift_uploadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, uint32_t nb_feats, uint32_t gpu_buffer_id)
{
  if (!isBufferIdxValid(instance, gpu_buffer_id) || !isInputFeatureCoundValid(instance, nb_feats))
  {
    logError(LOG_TAG, "vksift_uploadFeatures() error: invalid input.");
    abort();
  }

  // If a GPU task is currently using the buffer, wait for it to be available
  VkFence fences[1] = {instance->sift_memory->sift_buffer_fence_arr[gpu_buffer_id]};
  vkWaitForFences(instance->vulkan_device->device, 1, fences, VK_TRUE, UINT64_MAX);

  if (!vksift_Memory_copyBufferFeaturesToGPU(instance->sift_memory, gpu_buffer_id, feats_ptr, nb_feats))
  {
    logError(LOG_TAG, "vksift_uploadFeatures() error when uploading SIFT features to GPU memory.");
    abort();
  }
}

void vksift_matchFeatures(vksift_Instance instance, uint32_t gpu_buffer_id_A, uint32_t gpu_buffer_id_B)
{
  if (!isBufferIdxValid(instance, gpu_buffer_id_A) || !isBufferIdxValid(instance, gpu_buffer_id_B))
  {
    logError(LOG_TAG, "vksift_matchFeatures() error: invalid input.");
    abort();
  }

  // If a GPU task is currently using the buffers or the matching pipeline, wait for them to be available
  VkFence fences[3] = {instance->sift_memory->sift_buffer_fence_arr[gpu_buffer_id_A], instance->sift_memory->sift_buffer_fence_arr[gpu_buffer_id_B],
                       instance->sift_matcher->end_of_matching_fence};
  vkWaitForFences(instance->vulkan_device->device, 3, fences, VK_TRUE, UINT64_MAX);

  if (vksift_prepareSiftMemoryForMatching(instance->sift_memory, gpu_buffer_id_A, gpu_buffer_id_B))
  {
    vksift_dispatchSiftMatching(instance->sift_matcher, gpu_buffer_id_A, gpu_buffer_id_B);
  }
  else
  {
    logError(LOG_TAG, "vksift_matchFeatures() error: Failed to prepare the SIFT buffers for the matching pipeline.");
    abort();
  }
}

uint32_t vksift_getMatchesNumber(vksift_Instance instance)
{
  uint32_t nb_matches = 0u;
  vksift_Memory_getBufferMatchesCount(instance->sift_memory, &nb_matches);
  return nb_matches;
}

void vksift_downloadMatches(vksift_Instance instance, vksift_Match_2NN *matches)
{
  // If a GPU matching pipeline is currently using the matches buffer, wait for the pipeline to end
  VkFence fences[1] = {instance->sift_matcher->end_of_matching_fence};
  vkWaitForFences(instance->vulkan_device->device, 1, fences, VK_TRUE, UINT64_MAX);

  if (!vksift_Memory_copyBufferMatchesFromGPU(instance->sift_memory, matches))
  {
    logError(LOG_TAG, "vksift_downloadMatches() error when downloading SIFT matches from GPU memory.");
    abort();
  }
}

///////////////////////////////////////////////////////////////////////
// Scale-space access functions (for debug and visualization)
uint8_t vksift_getScaleSpaceNbOctaves(vksift_Instance instance) { return instance->sift_memory->curr_nb_octaves; }

void vksift_getScaleSpaceOctaveResolution(vksift_Instance instance, const uint8_t octave, uint32_t *octave_images_width, uint32_t *octave_images_height)
{
  if (octave > instance->sift_memory->curr_nb_octaves)
  {
    logError(LOG_TAG, "vksift_getScaleSpaceOctaveResolution() error: invalid input. Requested octave idx is %d but the current number of octave is %d",
             octave, instance->sift_memory->curr_nb_octaves);
    abort();
  }
  *octave_images_width = instance->sift_memory->octave_resolutions[octave].width;
  *octave_images_height = instance->sift_memory->octave_resolutions[octave].height;
}

void vksift_downloadScaleSpaceImage(vksift_Instance instance, const uint8_t octave, const uint8_t scale, float *blurred_image)
{
  if (!isInputOctaveIdxValid(instance, octave) || !isInputScaleIdxValid(instance, scale, false))
  {
    logError(LOG_TAG, "vksift_downloadScaleSpaceImage() error: invalid input.");
  }

  // Images cannot be transferred when a detection is running, wait for the fence to be sure this is not the case
  VkFence fences[1] = {instance->sift_detector->end_of_detection_fence};
  vkWaitForFences(instance->vulkan_device->device, 1, fences, VK_TRUE, UINT64_MAX);

  if (!vksift_Memory_copyPyramidImageFromGPU(instance->sift_memory, octave, scale, false, blurred_image))
  {
    logError(LOG_TAG, "vksift_downloadScaleSpaceImage() error when downloading pyramid blurred image from GPU memory.");
    abort();
  }
}

void vksift_downloadDoGImage(vksift_Instance instance, const uint8_t octave, const uint8_t scale, float *dog_image)
{
  if (!isInputOctaveIdxValid(instance, octave) || !isInputScaleIdxValid(instance, scale, true))
  {
    logError(LOG_TAG, "vksift_downloadDoGImage() error: invalid input.");
  }

  // Images cannot be transferred when a detection is running, wait for the fence to be sure this is not the case
  VkFence fences[1] = {instance->sift_detector->end_of_detection_fence};
  vkWaitForFences(instance->vulkan_device->device, 1, fences, VK_TRUE, UINT64_MAX);

  if (!vksift_Memory_copyPyramidImageFromGPU(instance->sift_memory, octave, scale, true, dog_image))
  {
    logError(LOG_TAG, "vksift_downloadDoGImage() error when downloading pyramid DoG image from GPU memory.");
    abort();
  }
}
///////////////////////////////////////////////////////////////////////
