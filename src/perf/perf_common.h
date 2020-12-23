#ifndef PERF_COMMON_H
#define PERF_COMMON_H

#include <opencv2/opencv.hpp>

#include "perf/wrappers/wrapper.h"

#define LOWES_RATIO 0.75

class AbstractSiftDetector;

enum DETECTOR_TYPE
{
  VULKANSIFT,
  OPENCV,
  VLFEAT,
  SIFTGPU
};

struct CommonPoint
{
  float x;
  float y;
};

std::vector<std::string> getDetectorTypeNames();
bool getDetectorTypeFromName(std::string det_name, DETECTOR_TYPE &det_type);
std::shared_ptr<AbstractSiftDetector> createDetector(DETECTOR_TYPE type);

#endif // PERF_COMMON_H