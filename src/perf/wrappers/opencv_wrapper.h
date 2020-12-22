#ifndef PERF_OPENCV_WRAPPER
#define PERF_OPENCV_WRAPPER

#include "perf/wrappers/wrapper.h"

class OpenCvDetector : public AbstractSiftDetector
{
  public:
  OpenCvDetector();

  bool init() override;
  void terminate() override;
  void getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                  std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2);
  float measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter) override;

  private:
  cv::Ptr<cv::SIFT> m_detector;
  cv::Ptr<cv::BFMatcher> m_matcher;
};

#endif // PERF_OPENCV_WRAPPER