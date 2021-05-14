#ifndef VULKAN_SIFT_H
#define VULKAN_SIFT_H

#include "vulkansift/types.h"

#include <stdbool.h>
#include <stdint.h>

bool vksift_loadVulkan();
void vksift_unloadVulkan();
void vksift_getAvailableGPUs(uint32_t *gpu_count, char *gpu_names[256]);
void vksift_setLogLevel(vksift_LogLevel level);

typedef struct vksift_Instance_T *vksift_Instance;
bool vksift_createInstance(vksift_Instance *instance_ptr, const vksift_Config *config);
void vksift_destroyInstance(vksift_Instance *instance_ptr);

void vksift_detectFeatures(vksift_Instance instance, const uint8_t *image_data, const uint32_t image_width, const uint32_t image_height,
                           const uint32_t gpu_buffer_id);
uint32_t vksift_getFeaturesNumber(vksift_Instance instance, const uint32_t gpu_buffer_id);
void vksift_downloadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, uint32_t gpu_buffer_id);

// Upload SIFT to GPU buffers
void vksift_uploadFeatures(vksift_Instance instance, vksift_Feature *feats_ptr, uint32_t nb_feats, uint32_t gpu_buffer_id);

// Match SIFT features from two buffers
void vksift_matchFeatures(vksift_Instance instance, uint32_t gpu_buffer_id_A, uint32_t gpu_buffer_id_B);
uint32_t vksift_getMatchesNumber(vksift_Instance instance);
void vksift_downloadMatches(vksift_Instance instance, vksift_Match_2NN *matches);

#if !defined(NDEBUG) && defined(VKSIFT_GPU_DEBUG)
// Draw an empty frame in the debug window. Necessary to use graphics GPU debuggers/profilers such as RenderDoc or Nvidia Nsight
// (They use frame delimiters to detect when to start/stop debugging and can't detect compute-only applications)
bool vksift_presentDebugFrame(vksift_Instance instance);
#endif

///////////////////////////////////////////////////////////////////////
// Scale-space access functions (for debug and visualization)
uint8_t vksift_getScaleSpaceNbOctaves();
void vksift_getScaleSpaceOctaveResolution(const uint8_t octave, uint32_t *octave_images_width, uint32_t *octave_images_height);
void vksift_downloadScaleSpaceImage(const uint8_t octave, const uint8_t scale, const float *blurred_image);
void vksift_downloadDoGImage(const uint8_t octave, const uint8_t scale, const float *blurred_image);
///////////////////////////////////////////////////////////////////////

#endif // VULKAN_SIFT_H