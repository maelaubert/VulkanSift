#ifndef PERF_WRAPPER_H
#define PERF_WRAPPER_H

#include "perf/perf_common.h"
#include <opencv2/opencv.hpp>

#include <chrono>

struct CommonPoint;

class AbstractSiftDetector
{
  public:
  virtual bool init() = 0;
  virtual void terminate() = 0;
  virtual void getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                          std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2) = 0;
  virtual float measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter) = 0;
};

#endif // PERF_WRAPPER_H