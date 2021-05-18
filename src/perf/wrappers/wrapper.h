#ifndef PERF_WRAPPER_H
#define PERF_WRAPPER_H

#include "perf/perf_common.h"
#include <opencv2/opencv.hpp>

#include <chrono>

class AbstractSiftDetector
{
  public:
  virtual bool init() = 0;
  virtual void terminate() = 0;
  // convert_and_copy_to_cv_format used to avoid transforming data when running runtime evaluation. Since every detector format is different
  // the transformation changes and might be slower than other detectors. If set to false, we stop when data is available on the CPU in the
  // detector's format but output data structures are not filled.
  virtual void detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format) = 0;
  virtual bool useFloatImage() = 0;
  virtual ~AbstractSiftDetector() = default;
};

#endif // PERF_WRAPPER_H