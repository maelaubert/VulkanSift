#ifndef PERF_COMMON_H
#define PERF_COMMON_H

#include <opencv2/opencv.hpp>

#include "perf/wrappers/wrapper.h"

#define LOWES_RATIO 0.75

class AbstractSiftDetector;

enum DETECTOR_TYPE
{
  VULKANSIFT,
  OPENCV,
  VLFEAT,
  SIFTGPU,
  POPSIFT
};

struct CommonPoint
{
  float x;
  float y;
};

std::vector<std::string> getDetectorTypeNames();
bool getDetectorTypeFromName(std::string det_name, DETECTOR_TYPE &det_type);
std::shared_ptr<AbstractSiftDetector> createDetector(DETECTOR_TYPE type);

// SIFT matching method
void matchFeatures(cv::Mat image1, cv::Mat image2, std::vector<cv::KeyPoint> &kps_img1, cv::Mat desc_img1, std::vector<cv::KeyPoint> &kps_img2,
                   cv::Mat desc_img2, std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2, bool do_crosscheck);

#endif // PERF_COMMON_H