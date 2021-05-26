#ifndef PERF_POPSIFT_WRAPPER_H
#define PERF_POPSIFT_WRAPPER_H

#include "perf/wrappers/wrapper.h"
#include "popsift/src/popsift.h"

class PopSiftDetector : public AbstractSiftDetector
{
  public:
  bool init() override;
  void terminate() override;
  void detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format) override;
  bool useFloatImage() override { return false; }

  private:
  std::shared_ptr<PopSift> detector;
};

#endif // PERF_POPSIFT_WRAPPER_H