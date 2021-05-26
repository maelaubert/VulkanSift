#ifndef PERF_OPENCV_WRAPPER
#define PERF_OPENCV_WRAPPER

#include "perf/wrappers/wrapper.h"

class OpenCvDetector : public AbstractSiftDetector
{
  public:
  OpenCvDetector();

  bool init() override;
  void terminate() override;
  void detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format) override;
  bool useFloatImage() override { return false; }

  private:
  cv::Ptr<cv::SIFT> m_detector;
  cv::Ptr<cv::BFMatcher> m_matcher;
};

#endif // PERF_OPENCV_WRAPPER