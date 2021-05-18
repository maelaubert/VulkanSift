#ifndef PERF_VUKANSIFT_WRAPPER_H
#define PERF_VUKANSIFT_WRAPPER_H

#include "perf/wrappers/wrapper.h"
extern "C"
{
#include <vulkansift/vulkansift.h>
}

class VulkanSiftDetector : public AbstractSiftDetector
{
  public:
  bool init() override;
  void terminate() override;
  void detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format) override;
  bool useFloatImage() override { return false; }

  private:
  vksift_Instance sift_instance;
};

#endif // PERF_VUKANSIFT_WRAPPER_H