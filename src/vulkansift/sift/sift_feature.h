#ifndef SIFT_FEATURE_H
#define SIFT_FEATURE_H

#include <cstdint>

namespace VulkanSIFT
{

struct SIFT_Feature
{
  uint32_t x;
  uint32_t y;
  float score;
  float angle;
  uint32_t desc[256 / 32];
};

} // namespace VulkanSIFT

#endif // SIFT_FEATURE_H
