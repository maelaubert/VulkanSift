#ifndef PERF_SIFTGPU_WRAPPER_H
#define PERF_SIFTGPU_WRAPPER_H

#include "perf/wrappers/wrapper.h"
#include <vector>

#include "SiftGPU/src/SiftGPU.h"

class SiftGPUDetector : public AbstractSiftDetector
{
  public:
  bool init() override;
  void terminate() override;
  void detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format) override;
  bool useFloatImage() override { return false; }

  private:
  SiftGPU sift;
};

#endif // PERF_SIFTGPU_WRAPPER_H