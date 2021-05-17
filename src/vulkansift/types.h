#ifndef VKSIFT_TYPES_H
#define VKSIFT_TYPES_H

#define VKSIFT_FEATURE_NB_HIST 4
#define VKSIFT_FEATURE_NB_ORI 8

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  // TODO remove unnecessary variables ()
  float x;
  float y;
  uint32_t orig_x; // TODO: switch to float for subpixel precision at original image res
  uint32_t orig_y;
  uint32_t index_scale;
  float sigma;
  float scale_factor;
  float theta;
  float value;
  uint8_t feature_vector[VKSIFT_FEATURE_NB_HIST * VKSIFT_FEATURE_NB_HIST * VKSIFT_FEATURE_NB_ORI];
} vksift_Feature;

typedef struct
{
  uint32_t idx_a;
  uint32_t idx_b1;
  uint32_t idx_b2;
  float dist_a_b1;
  float dist_a_b2;
} vksift_Match_2NN;

typedef enum
{
  VKSIFT_NO_LOG,
  VKSIFT_LOG_ERROR,
  VKSIFT_LOG_WARNING,
  VKSIFT_LOG_INFO,
  VKSIFT_LOG_DEBUG
} vksift_LogLevel;

typedef enum
{
  VKSIFT_PYRAMID_PRECISION_FLOAT16,
  VKSIFT_PYRAMID_PRECISION_FLOAT32,
} vksift_PyramidPrecisionMode;

typedef struct
{
  // Input/Output configuration

  // Maximum size (in bytes) for the input grayscale images
  // (defined as input_image_max_size = max_width*max_height)
  // (default: 1920*1080)
  uint32_t input_image_max_size;
  // Number of SIFT buffers (stored on the GPU) to be reserved by the application (default: 2)
  uint32_t sift_buffer_count;
  // Maximum number of SIFT features stored by a GPU SIFT bufer (default: 100000)
  uint32_t max_nb_sift_per_buffer;

  // SIFT algorithm configuration

  // If true, a 2x upscaled version of the input image will used for the Gaussian scale-space construction.
  // Using this option, more SIFT features will be found at the expense of a longer processing time. (default: true)
  bool use_input_upsampling;
  // Number of octaves used in the Gaussian scale-space. If set to 0, the number of octaves is defined by the implementation and
  // depends on the input image resolution. (default: 0)
  uint8_t nb_octaves;
  // Number of scales used per octave in the Gaussian scale-space. (default: 3)
  uint8_t nb_scales_per_octave;
  // Assumed blur level for the input image (default: 0.5f)
  float input_image_blur_level;
  // Blur level for the Gaussian scale-space seed scale (default: 1.6f)
  float seed_scale_sigma;
  // Minimum Difference of Gaussian image intensity threshold used to detect a SIFT keypoints (expressed in normalized intensity value [0.f..1.f])
  // In the implementation, this value is divided by nb_scales_per_octave before being used. (default: 0.04f)
  float intensity_threshold;
  // Edge threshold used to discard SIFT keypoints on Difference of Gaussian image edges (default: 10.f)
  float edge_threshold;
  // Max number of orientation per SIFT keypoint (one descriptor is detector for each orientation
  // If set to 0, no limit will be applied. (default: 0)
  uint8_t max_nb_orientation_per_keypoint;

  // GPU and implementation configuration (if <0 the GPU with highest expected performance is selected) (default: -1)
  int32_t gpu_device_index;
  // If true, the GPU hardware texture samplers will be used to speed up the Gaussian scale-space construction. (default: true)
  bool use_hardware_interpolated_blur;
  // Defines the scale-space image format precision (default: VKSIFT_PYRAMID_PRECISION_FLOAT32)
  vksift_PyramidPrecisionMode pyramid_precision_mode;
} vksift_Config;

static vksift_Config vksift_Config_Default = {.input_image_max_size = 1920u * 1080u,
                                              .sift_buffer_count = 2u, // minimum number of buffer to support the feature matching function
                                              .max_nb_sift_per_buffer = 100000u,
                                              .use_input_upsampling = true, // provide the best results (higher processing time)
                                              .nb_octaves = 0,              // defined by implementation
                                              .nb_scales_per_octave = 3u,   // Lowe's paper
                                              .input_image_blur_level = 0.5f,
                                              .seed_scale_sigma = 1.6f, // Lowe's paper
                                              .intensity_threshold = 0.04f,
                                              .edge_threshold = 10.f,                 // Lowe's paper
                                              .max_nb_orientation_per_keypoint = 0,   // no limit
                                              .gpu_device_index = -1,                 // GPU auto-selection
                                              .use_hardware_interpolated_blur = true, // faster with no noticeable quality loss
                                              .pyramid_precision_mode = VKSIFT_PYRAMID_PRECISION_FLOAT32};

#endif // VKSIFT_TYPES_H
