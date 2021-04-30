#include "vulkansift/vulkansift.h"

#include "vulkansift/vkenv/logger.h"
#include "vulkansift/vkenv/vulkan_device.h"
#include "vulkansift/vkenv/vulkan_utils.h"
#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
#include "vulkansift/vkenv/debug_presenter.h"
#include "vulkansift/vkenv/mini_window.h"
#endif

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
  case VKSIFT_LOG_INFO:
    vkenv_setLogLevel(VKENV_LOG_INFO);
    break;
  default:
    logError(LOG_TAG, "Unhandled vksift_LogLevel in vksift_setLogLevel()");
    break;
  }
}

typedef struct vksift_Instance_T
{
  vkenv_Device vulkan_device;

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
  config_valid &= checkConfigCond(config->constrast_threshold >= 0.f, "Invalid configuration: the DoG contrast threshold cannot be negative");
  config_valid &= checkConfigCond(config->edge_threshold >= 0.f, "Invalid configuration: the DoG edge threshold cannot be negative");
  config_valid &= checkConfigCond(config->edge_threshold >= 0.f, "Invalid configuration: the DoG edge threshold cannot be negative");

  switch (config->pyramid_precision_mode)
  {
  case VKSIFT_PYRAMID_PRECISION_FLOAT16:
    break;
  case VKSIFT_PYRAMID_PRECISION_FLOAT32:
    break;
  case VKSIFT_PYRAMID_PRECISION_FLOAT64:
    break;
  default:
    logError(LOG_TAG, "Invalid configuration: invalid scale-space pyramid format precision specified)");
    config_valid &= false;
    break;
  }

  return config_valid;
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
  vkenv_DeviceConfig gpu_config = {.device_extension_count = 0, .device_extensions = NULL, .target_device_idx = config->gpu_device_index};
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

  // TODO
  // Setup vksift_Memory
  // Setup Detector (gaussian kernels, descriptors, pipelines, cmdbufs, etc)
  // Setup Matcher (descriptors, pipelines, cmdbufs)
  return true;
}

void vksift_destroyInstance(vksift_Instance *instance_ptr)
{
  assert(instance_ptr != NULL);
  assert(*instance_ptr != NULL); // vksift_destroyInstance shouldn't be called on NULL Instance
  vksift_Instance instance = *instance_ptr;

  // Destroy DebugPresenter
  VK_NULL_SAFE_DELETE(instance->debug_presenter, vkenv_destroyDebugPresenter(instance->vulkan_device, &instance->debug_presenter));

  // Destroy Vulkan device
  VK_NULL_SAFE_DELETE(instance->vulkan_device, vkenv_destroyDevice(&instance->vulkan_device));

  // Releave vksift_Instance memory
  free(*instance_ptr);
  *instance_ptr = NULL;
}

#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
bool vksift_presentDebugFrame(vksift_Instance instance) { return vkenv_presentDebugFrame(instance->vulkan_device, instance->debug_presenter); }
#endif

void vksift_detectFeatures(vksift_Instance instance, const uint8_t *image_data, const uint32_t image_width, const uint32_t image_height,
                           const uint32_t gpu_buffer_id)
{
  // TODO
}

void vksift_getFeaturesNumber(vksift_Instance instance, const uint32_t gpu_buffer_id)
{
  // TODO
}
void vksift_downloadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, uint32_t gpu_buffer_id)
{
  // TODO
}

void vksift_uploadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, uint32_t nb_feats, uint32_t gpu_buffer_id)
{
  // TODO
}

void vksift_matchFeatures(vksift_Instance instance, uint32_t gpu_buffer_id_A, uint32_t gpu_buffer_id_B)
{
  // TODO
}

void vksift_downloadMatches(vksift_Instance instance, vksift_Feature *matches)
{
  // TODO
}