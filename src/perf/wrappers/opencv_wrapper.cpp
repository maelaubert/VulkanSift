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
  cv::Mat descs_f32;
  m_detector->detectAndCompute(image, cv::noArray(), keypoints, descs_f32);
  if (convert_and_copy_to_cv_format)
  {
    descs.create((int)keypoints.size(), 128, CV_8U);
    for (int i = 0; i < (int)keypoints.size(); i++)
    {
      for (int j = 0; j < 128; j++)
      {
        descs.at<uchar>(i, j) = static_cast<uchar>(descs_f32.at<float>(i, j));
      }
    }
  }
}