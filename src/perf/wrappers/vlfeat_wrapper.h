#ifndef PERF_VLFEAT_WRAPPER_H
#define PERF_VLFEAT_WRAPPER_H

#include "perf/wrappers/wrapper.h"
#include "vlfeat/src/sift.h"

class VLFeatDetector : public AbstractSiftDetector
{
  public:
  bool init();
  void terminate();
  void getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                  std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2);
  float measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter);

  private:
  float *allocAndFillGreyBufferFromCvMat(cv::Mat image);
};

#endif // PERF_VLFEAT_WRAPPER_H