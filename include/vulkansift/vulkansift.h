#ifndef VULKAN_SIFT_H
#define VULKAN_SIFT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vulkansift/vulkansift_types.h"

#include <stdbool.h>
#include <stdint.h>

  // Load/Unload the Vulkan API.
  // vksift_loadVulkan() must be called before the first VulkanSift function call and vksift_unloadVulkan() must be called after that last
  // VulkanSift function call.
  vksift_ErrorType vksift_loadVulkan();
  void vksift_unloadVulkan();

  // Retrieve the names of the available GPU (GPUs must provide a Vulkan driver to be visible)
  // If gpu_names is NULL, fill gpu_count with the number of available GPU.
  // If gpu_names is not NULL, copy the first gpu_count VKSIFT_GPU_NAMES to the gpu_names buffer.
  void vksift_getAvailableGPUs(uint32_t *gpu_count, VKSIFT_GPU_NAME *gpu_names);
  void vksift_setLogLevel(const vksift_LogLevel level);

  // Create and destroy a vksift_Instance. A vksift_Instance manages GPU resources, detection and matching pipelines as configured
  // by the vksift_Config user configuration. It uses only one GPU device specified in the configuration (if not specified
  // the best available GPU should be automatically selected).
  // vksift_ExternalWindowInfo pointer is only needed to debug/profile VulkanSift GPU functions and use vksift_presentDebugFrame(),
  // it can be left to NULL if not needed.
  typedef struct vksift_Instance_T *vksift_Instance;
#ifdef __cplusplus
  vksift_ErrorType vksift_createInstance(vksift_Instance *instance_ptr, const vksift_Config *config,
                                         const vksift_ExternalWindowInfo *external_window_info_ptr = NULL);
#else
vksift_ErrorType vksift_createInstance(vksift_Instance *instance_ptr, const vksift_Config *config,
                                       const vksift_ExternalWindowInfo *external_window_info_ptr);
#endif
  void vksift_destroyInstance(vksift_Instance *instance_ptr);
  vksift_Config vksift_getDefaultConfig();

  /**
   * Detection/Matching pipelines
   *
   * vksift_detectFeatures() and vksift_matchFeatures() are both NON-BLOCKING. This means that the functions return as soon as the
   * detection/matching pipelines are started without waiting for the results to be available.
   **/

  // Copy the image to the GPU and start the detection pipeline on the GPU. Detected features will be stored on the
  // specified GPU buffer.
  void vksift_detectFeatures(vksift_Instance instance, const uint8_t *image_data, const uint32_t image_width, const uint32_t image_height,
                             const uint32_t gpu_buffer_id);

  // For each SIFT feature in the buffer A, find the 2-nearest neighbors in the buffer B, store feature index and descriptors L2 distance
  // for the two neighbors.
  void vksift_matchFeatures(vksift_Instance instance, const uint32_t gpu_buffer_id_A, const uint32_t gpu_buffer_id_B);

  /**
   * Data transfer functions
   *
   * All transfer functions are BLOCKING. This means that the functions return when the requested data is made available to the user.
   * If the requested data is currently being used or generated by the detection/matching pipeline the function will block until
   * the detection/matching process is done.
   *
   * Note: if your GPU support async-tranfer operations, the data-transfer can be executed by the GPU while another pipeline is running,
   * otherwise the GPU may complete other operations before doing the transfer (defined by vendor).
   **/

  // Return the number of features available in the specified GPU buffer.
  uint32_t vksift_getFeaturesNumber(vksift_Instance instance, const uint32_t gpu_buffer_id);
  // Download SIFT features from the specified GPU buffer and copy them to feats_ptr.
  void vksift_downloadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, const uint32_t gpu_buffer_id);
  // Upload SIFT features to the specified GPU buffer.
  void vksift_uploadFeatures(vksift_Instance instance, const vksift_Feature *feats_ptr, const uint32_t nb_feats, const uint32_t gpu_buffer_id);
  // Return the number of matches found. (same as the number of features in the buffer A used in the last call to vksift_matchFeatures())
  uint32_t vksift_getMatchesNumber(vksift_Instance instance);
  void vksift_downloadMatches(vksift_Instance instance, vksift_Match_2NN *matches);

  // Get the buffer availability status. Return true if the GPU is not using the buffer for a detection/matching task, false otherwise.
  bool vksift_isBufferAvailable(vksift_Instance instance, const uint32_t gpu_buffer_id);

  // Scale-space access functions (for debug and visualization)
  // Return the current number of octave used (depends on configuration and input image resolution).
  uint8_t vksift_getScaleSpaceNbOctaves(vksift_Instance instance);
  // Return the image resolution used for the specified octave.
  void vksift_getScaleSpaceOctaveResolution(vksift_Instance instance, const uint8_t octave, uint32_t *octave_images_width, uint32_t *octave_images_height);
  // Copy the selected Gaussian image data to the blurred_image float buffer. (scale value in [0, config.nb_scales_per_octave+2])
  void vksift_downloadScaleSpaceImage(vksift_Instance instance, const uint8_t octave, const uint8_t scale, float *blurred_image);
  // Copy the selected Difference of Gaussian image data to the blurred_image float buffer. (scale value in [0, config.nb_scales_per_octave+1])
  void vksift_downloadDoGImage(vksift_Instance instance, const uint8_t octave, const uint8_t scale, float *dog_image);

  /**
   * GPU Debug functions
   *
   * WARNING | Only available when external window information were specified in vksift_createInstance.
   *         | Do nothing and print a warning if this is not the case.
   **/
  // Draw an empty frame in the debug window. Required to use graphics GPU debuggers/profilers such as RenderDoc or Nvidia Nsight
  // (They use frame delimiters to detect when to start/stop debugging and can't detect compute-only applications)
  void vksift_presentDebugFrame(vksift_Instance instance);

#ifdef __cplusplus
}
#endif

#endif // VULKAN_SIFT_H