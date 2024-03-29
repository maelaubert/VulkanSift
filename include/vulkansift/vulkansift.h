#ifndef VULKAN_SIFT_H
#define VULKAN_SIFT_H

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_WIN32) || defined(_WIN64)
#define VKSIFT_EXPORT __declspec(dllexport)
#else
#define VKSIFT_EXPORT __attribute__((__visibility__("default")))
#endif

#include "vulkansift/vulkansift_types.h"

#include <stdbool.h>
#include <stdint.h>

  // Load/Unload the Vulkan API.
  // vksift_loadVulkan() must be called before the first VulkanSift function call and vksift_unloadVulkan() must be called after that last
  // VulkanSift function call.
  VKSIFT_EXPORT vksift_Result vksift_loadVulkan();
  VKSIFT_EXPORT void vksift_unloadVulkan();

  // Retrieve the name of the available GPU(s) (GPUs must support Vulkan to be visible)
  // If gpu_names is NULL, fill gpu_count with the number of available GPU.
  // If gpu_names is not NULL, copy a number of VKSIFT_GPU_NAMES defined by gpu_count to the gpu_names buffer.
  VKSIFT_EXPORT void vksift_getAvailableGPUs(uint32_t *gpu_count, VKSIFT_GPU_NAME *gpu_names);
  VKSIFT_EXPORT void vksift_setLogLevel(const vksift_LogLevel level);

  // Create and destroy a vksift_Instance. A vksift_Instance manages GPU resources, detection and matching pipelines as configured
  // by the vksift_Config user configuration. It uses only one GPU device specified in the configuration (if not specified
  // the best available GPU should be automatically selected).
  typedef struct vksift_Instance_T *vksift_Instance;
  VKSIFT_EXPORT vksift_Result vksift_createInstance(vksift_Instance *instance_ptr, const vksift_Config *config);
  VKSIFT_EXPORT void vksift_destroyInstance(vksift_Instance *instance_ptr);
  VKSIFT_EXPORT vksift_Config vksift_getDefaultConfig();

  /**
   * Detection/Matching pipelines
   *
   * vksift_detectFeatures() and vksift_matchFeatures() calls DO NOT WAIT for the results to be available.
   * This means that the functions return as soon as the detection/matching pipelines are started without waiting
   * for the results to be available. However, is a detection/matching pipeline is already running, it must wait
   * the end of the previous pipeline before starting the new one.
   **/

  // Copy the image to the GPU and start the detection pipeline on the GPU. Detected features will be stored on the
  // specified GPU buffer. The parameter image_data must point to an array of uint8_t values representing a grayscale image (row-major).
  VKSIFT_EXPORT void vksift_detectFeatures(vksift_Instance instance, const uint8_t *image_data, const uint32_t image_width, const uint32_t image_height,
                                           const uint32_t gpu_buffer_id);

  // For each SIFT feature in the buffer A, find the 2-nearest neighbors in the buffer B, store feature index and descriptors L2 distance
  // for the two neighbors.
  VKSIFT_EXPORT void vksift_matchFeatures(vksift_Instance instance, const uint32_t gpu_buffer_id_A, const uint32_t gpu_buffer_id_B);

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
  VKSIFT_EXPORT uint32_t vksift_getFeaturesNumber(vksift_Instance instance, const uint32_t gpu_buffer_id);

  // Download SIFT features from the specified GPU buffer and copy them to feats_ptr. The parameter feats_ptr must provide enough space to
  // store all the features present in the SIFT buffer (number of features retrieved with vksift_getFeaturesNumber())
  VKSIFT_EXPORT void vksift_downloadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, const uint32_t gpu_buffer_id);

  // Upload SIFT features to the specified GPU buffer.
  VKSIFT_EXPORT void vksift_uploadFeatures(vksift_Instance instance, const vksift_Feature *feats_ptr, const uint32_t nb_feats,
                                           const uint32_t gpu_buffer_id);

  // Return the number of matches found.
  VKSIFT_EXPORT uint32_t vksift_getMatchesNumber(vksift_Instance instance);

  // Copy GPU matches information (vksift_Match_2NN) to the matches buffer. The parameter matches must provide enough space to store
  // all the matches (number of matches retrieved with vksift_getMatchesNumber())
  VKSIFT_EXPORT void vksift_downloadMatches(vksift_Instance instance, vksift_Match_2NN *matches);

  // Get the buffer availability status. Return true if the GPU is not using the buffer for a detection/matching task, false otherwise.
  // Can be used to check for the result or device availability and avoid long CPU blocking calls.
  VKSIFT_EXPORT bool vksift_isBufferAvailable(vksift_Instance instance, const uint32_t gpu_buffer_id);

  // Scale-space access functions (for debug and visualization)
  // Return the current number of octave used (depends on configuration and input image resolution).
  VKSIFT_EXPORT uint8_t vksift_getScaleSpaceNbOctaves(vksift_Instance instance);
  // Return the image resolution used for the specified octave.
  VKSIFT_EXPORT void vksift_getScaleSpaceOctaveResolution(vksift_Instance instance, const uint8_t octave, uint32_t *octave_images_width,
                                                          uint32_t *octave_images_height);
  // Copy the selected Gaussian image data to the blurred_image float buffer. (scale value in [0, config.nb_scales_per_octave+3[ )
  VKSIFT_EXPORT void vksift_downloadScaleSpaceImage(vksift_Instance instance, const uint8_t octave, const uint8_t scale, float *blurred_image);
  // Copy the selected Difference of Gaussian image data to the blurred_image float buffer. (scale value in [0, config.nb_scales_per_octave+2[ )
  VKSIFT_EXPORT void vksift_downloadDoGImage(vksift_Instance instance, const uint8_t octave, const uint8_t scale, float *dog_image);

  /**
   * GPU Debug functions
   *
   * WARNING | Only available when use_gpu_debug_functions is true and gpu_debug_external_window_info is filled
   *         | in the configuration structure during the call to vksift_createInstance.
   *         | Do nothing and print a warning if this is not the case.
   **/
  // Draw an empty frame in the debug window. Required to use graphics GPU debuggers/profilers such as RenderDoc or Nvidia Nsight
  // (They use frame delimiters to detect when to start/stop debugging and can't detect compute-only applications)
  VKSIFT_EXPORT void vksift_presentDebugFrame(vksift_Instance instance);

#ifdef __cplusplus
}
#endif

#endif // VULKAN_SIFT_H