#ifndef PERF_POPSIFT_WRAPPER_H
#define PERF_POPSIFT_WRAPPER_H

#include "perf/wrappers/wrapper.h"

class PopSiftDetector : public AbstractSiftDetector
{
  public:
  bool init();
  void terminate();
  void getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                  std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2);
  float measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter);

  private:
  uint8_t *allocAndFillGreyBufferFromCvMat(cv::Mat image);
};

#endif // PERF_POPSIFT_WRAPPER_H