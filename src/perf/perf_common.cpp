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
  if (det_name == std::string{"VulkanSift"})
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

cv::Mat getMatchesImage(uint8_t *in_img1, std::vector<CommonPoint> kps1, int width1, int height1, uint8_t *in_img2, std::vector<CommonPoint> kps2,
                        int width2, int height2)
{
  // Convert image 1
  cv::Mat output_mat1(height1, width1, CV_8U);
  for (int i = 0; i < width1 * height1; i++)
  {
    output_mat1.data[i] = in_img1[i];
  }
  cv::Mat output_mat_rgb1(height1, width1, CV_8UC3);
  output_mat1.convertTo(output_mat_rgb1, CV_8UC3);
  cv::cvtColor(output_mat_rgb1, output_mat_rgb1, cv::COLOR_GRAY2BGR);

  // Convert image 2
  cv::Mat output_mat2(height2, width2, CV_8U);
  for (int i = 0; i < width2 * height2; i++)
  {
    output_mat2.data[i] = in_img2[i];
  }
  cv::Mat output_mat_rgb2(height2, width2, CV_8UC3);
  output_mat2.convertTo(output_mat_rgb2, CV_8UC3);
  cv::cvtColor(output_mat_rgb2, output_mat_rgb2, cv::COLOR_GRAY2BGR);

  // Create concatenated image (update images to have the same size)
  int max_width = std::max(width1, width2);
  int max_height = std::max(height1, height2);
  cv::copyMakeBorder(output_mat_rgb1, output_mat_rgb1, 0, max_height - height1, 0, max_width - width1, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
  cv::copyMakeBorder(output_mat_rgb2, output_mat_rgb2, 0, max_height - height2, 0, max_width - width2, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

  cv::Mat concatenated_mat;
  cv::hconcat(output_mat_rgb1, output_mat_rgb2, concatenated_mat);

  srand(time(NULL));

  for (size_t i = 0; i < kps1.size(); i++)
  {
    cv::Scalar color(rand() % 255, rand() % 255, rand() % 255, rand() % 255);
    cv::Point im1_p(kps1.at(i).x, kps1.at(i).y);
    cv::Point im2_origin(max_width, 0);
    cv::Point im2_p(kps2.at(i).x, kps2.at(i).y);
    cv::line(concatenated_mat, im1_p, im2_origin + im2_p, color, 2, cv::LINE_AA);
  }

  return concatenated_mat;
}

void matchFeatures(cv::Mat image1, cv::Mat image2, std::vector<cv::KeyPoint> &kps_img1, cv::Mat desc_img1, std::vector<cv::KeyPoint> &kps_img2,
                   cv::Mat desc_img2, std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2, bool do_crosscheck)
{
  // Find feature matches
  std::vector<std::vector<cv::DMatch>> matches12, matches21;
  auto matcher = cv::BFMatcher::create(cv::NORM_L2);
  matcher->knnMatch(desc_img1, desc_img2, matches12, 2);
  if (do_crosscheck)
  {
    matcher->knnMatch(desc_img2, desc_img1, matches21, 2);
  }

  std::vector<cv::KeyPoint> kpt_filtered_im1, kpt_filtered_im2;

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
            kpt_filtered_im1.push_back(kps_img1[matches12[i][0].queryIdx]);
            kpt_filtered_im2.push_back(kps_img2[matches12[i][0].trainIdx]);
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
        kpt_filtered_im1.push_back(kps_img1[matches12[i][0].queryIdx]);
        kpt_filtered_im2.push_back(kps_img2[matches12[i][0].trainIdx]);
        matches_img1.push_back({pt_img1.x, pt_img1.y});
        matches_img2.push_back({pt_img2.x, pt_img2.y});
      }
    }
  }

  // Draw matches (for debug only)
  cv::Mat match_img = getMatchesImage(image1.data, matches_img1, image1.cols, image1.rows, image2.data, matches_img2, image2.cols, image2.rows);
  cv::imshow("Matches", match_img);
  cv::waitKey(30);
}