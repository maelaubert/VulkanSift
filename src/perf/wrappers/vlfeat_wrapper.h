#ifndef PERF_VLFEAT_WRAPPER_H
#define PERF_VLFEAT_WRAPPER_H

#include "perf/wrappers/wrapper.h"
#include "vlfeat/src/sift.h"

class VLFeatDetector : public AbstractSiftDetector
{
  public:
  bool init() override;
  void terminate() override;
  void detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format) override;
  bool useFloatImage() override { return true; }
};

#endif // PERF_VLFEAT_WRAPPER_H