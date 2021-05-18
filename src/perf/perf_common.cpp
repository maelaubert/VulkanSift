#include "perf/perf_common.h"
#include "perf/wrappers/opencv_wrapper.h"
#include "perf/wrappers/popsift_wrapper.h"
#include "perf/wrappers/siftgpu_wrapper.h"
#include "perf/wrappers/vlfeat_wrapper.h"
#include "perf/wrappers/vulkansift_wrapper.h"

static const std::vector<std::string> detector_name_list = {"VulkanSIFT", "OpenCV", "VLFeat", "SiftGPU", "PopSift"};

std::vector<std::string> getDetectorTypeNames() { return detector_name_list; }

bool getDetectorTypeFromName(std::string det_name, DETECTOR_TYPE &det_type)
{
  if (det_name == std::string{"VulkanSIFT"})
  {
    det_type = DETECTOR_TYPE::VULKANSIFT;
    return true;
  }
  else if (det_name == std::string{"OpenCV"})
  {
    det_type = DETECTOR_TYPE::OPENCV;
    return true;
  }
  else if (det_name == std::string{"VLFeat"})
  {
    det_type = DETECTOR_TYPE::VLFEAT;
    return true;
  }
  else if (det_name == std::string{"SiftGPU"})
  {
    det_type = DETECTOR_TYPE::SIFTGPU;
    return true;
  }
  else if (det_name == std::string{"PopSift"})
  {
    det_type = DETECTOR_TYPE::POPSIFT;
    return true;
  }
  return false;
}

std::shared_ptr<AbstractSiftDetector> createDetector(DETECTOR_TYPE type)
{
  switch (type)
  {
  case DETECTOR_TYPE::VULKANSIFT:
    return std::make_shared<VulkanSiftDetector>();
    break;
  case DETECTOR_TYPE::OPENCV:
    return std::make_shared<OpenCvDetector>();
  case DETECTOR_TYPE::VLFEAT:
    return std::make_shared<VLFeatDetector>();
  case DETECTOR_TYPE::SIFTGPU:
    return std::make_shared<SiftGPUDetector>();
  case DETECTOR_TYPE::POPSIFT:
    return std::make_shared<PopSiftDetector>();
  default:
    break;
  }
}

void matchFeatures(std::vector<cv::KeyPoint> &kps_img1, cv::Mat desc_img1, std::vector<cv::KeyPoint> &kps_img2, cv::Mat desc_img2,
                   std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2, bool do_crosscheck)
{
  // Find feature matches
  std::vector<std::vector<cv::DMatch>> matches12, matches21;
  auto matcher = cv::BFMatcher::create(cv::NORM_L2);
  matcher->knnMatch(desc_img1, desc_img2, matches12, 2);
  if (do_crosscheck)
  {
    matcher->knnMatch(desc_img2, desc_img1, matches21, 2);
  }

  if (do_crosscheck)
  {
    // Fill Match vector
    matches_img1.clear();
    matches_img2.clear();
    for (int i = 0; i < (int)matches12.size(); i++)
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
            cv::Point2f pt_img1 = kps_img1[matches12[i][0].queryIdx].pt;
            cv::Point2f pt_img2 = kps_img2[matches12[i][0].trainIdx].pt;
            matches_img1.push_back({pt_img1.x, pt_img1.y});
            matches_img2.push_back({pt_img2.x, pt_img2.y});
          }
        }
      }
    }
  }
  else
  {
    // Fill Match vector
    matches_img1.clear();
    matches_img2.clear();
    for (int i = 0; i < (int)matches12.size(); i++)
    {
      // Check Lowe's ratio in 1
      if ((matches12[i][0].distance / matches12[i][1].distance) < LOWES_RATIO)
      {
        cv::Point2f pt_img1 = kps_img1[matches12[i][0].queryIdx].pt;
        cv::Point2f pt_img2 = kps_img2[matches12[i][0].trainIdx].pt;
        matches_img1.push_back({pt_img1.x, pt_img1.y});
        matches_img2.push_back({pt_img2.x, pt_img2.y});
      }
    }
  }
}