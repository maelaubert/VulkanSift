#include "perf/wrappers/opencv_wrapper.h"

OpenCvDetector::OpenCvDetector()
{
  m_detector = cv::SIFT::create();
  m_matcher = cv::BFMatcher::create(cv::NORM_L2);
}

bool OpenCvDetector::init() { return true; }
void OpenCvDetector::terminate() {}

void OpenCvDetector::detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format)
{
  // Compute SIFT features on both images
  m_detector->detectAndCompute(image, cv::noArray(), keypoints, descs);
}