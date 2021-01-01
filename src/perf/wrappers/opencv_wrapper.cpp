#include "perf/wrappers/opencv_wrapper.h"

OpenCvDetector::OpenCvDetector()
{
  m_detector = cv::SIFT::create();
  m_matcher = cv::BFMatcher::create(cv::NORM_L2);
}

bool OpenCvDetector::init() { return true; }
void OpenCvDetector::terminate() {}

void OpenCvDetector::getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                                std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2)
{
  // Compute SIFT features on both images
  std::vector<cv::KeyPoint> img1_kps, img2_kps;
  cv::Mat descs1, descs2;
  m_detector->detectAndCompute(image1, cv::noArray(), img1_kps, descs1);
  m_detector->detectAndCompute(image2, cv::noArray(), img2_kps, descs2);

  kps_img1.clear();
  kps_img2.clear();
  for (auto kp : img1_kps)
  {
    kps_img1.push_back({kp.pt.x, kp.pt.y});
  }
  for (auto kp : img2_kps)
  {
    kps_img2.push_back({kp.pt.x, kp.pt.y});
  }

  // Find feature matches
  std::vector<std::vector<cv::DMatch>> matches12, matches21;
  auto matcher = cv::BFMatcher::create(cv::NORM_L2);
  matcher->knnMatch(descs1, descs2, matches12, 2);
  matcher->knnMatch(descs2, descs1, matches21, 2);

  // Fill Match vector
  matches_img1.clear();
  matches_img2.clear();
  for (int i = 0; i < matches12.size(); i++)
  {
    int idx_in_2 = matches12[i][0].trainIdx;
    // Check mutual match
    if (matches21[idx_in_2][0].trainIdx == i)
    {
      // Check Lowe's ratio in 1
      if ((matches12[i][0].distance / matches12[i][1].distance) < LOWES_RATIO)
      {
        // Check Lowe's ratio in 2
        if ((matches21[idx_in_2][0].distance / matches21[idx_in_2][1].distance) < LOWES_RATIO)
        {
          matches_img1.push_back(kps_img1[matches12[i][0].queryIdx]);
          matches_img2.push_back(kps_img2[matches12[i][0].trainIdx]);
        }
      }
    }
  }
}

float OpenCvDetector::measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter)
{
  std::vector<cv::KeyPoint> img_kps;
  cv::Mat img_descs;
  for (int i = 0; i < nb_warmup_iter; i++)
  {
    m_detector->detectAndCompute(image, cv::noArray(), img_kps, img_descs);
    std::cout << "\rWarmup " << i + 1 << "/" << nb_warmup_iter;
  }
  std::cout << std::endl;

  float sum_duration = 0;
  for (int i = 0; i < nb_iter; i++)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    m_detector->detectAndCompute(image, cv::noArray(), img_kps, img_descs);
    float duration =
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count()) / 1000.f;
    sum_duration += duration;
    std::cout << "\rMeasuring " << i + 1 << "/" << nb_iter;
  }
  std::cout << std::endl;

  float mean_duration = float(sum_duration) / float(nb_iter);
  return mean_duration;
}
