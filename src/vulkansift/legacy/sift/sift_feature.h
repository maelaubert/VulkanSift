#ifndef SIFT_FEATURE_H
#define SIFT_FEATURE_H

#define SIFT_NB_HIST 4
#define SIFT_NB_ORI 8

#include <cstdint>

namespace VulkanSIFT
{

struct SIFT_Feature
{
  float x;
  float y;
  uint32_t orig_x;
  uint32_t orig_y;
  uint32_t index_scale;
  float sigma;
  float scale_factor;
  float theta;
  float value;
  uint8_t feature_vector[SIFT_NB_HIST * SIFT_NB_HIST * SIFT_NB_ORI];
};

} // namespace VulkanSIFT

#endif // SIFT_FEATURE_H
