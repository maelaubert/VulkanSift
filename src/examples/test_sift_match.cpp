#include "test_utils.h"
#include <vulkansift/vulkansift.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    std::cout << "Invalid command." << std::endl;
    std::cout << "Usage: ./test_sift_match PATH_TO_IMAGE1 PATH_TO_IMAGE2" << std::endl;
    return -1;
  }

  // Read image with OpenCV
  cv::Mat img1 = cv::imread(argv[1], 0);
  if (img1.empty())
  {
    std::cout << "Failed to read image 1 " << argv[1] << ". Stopping program." << std::endl;
    return -1;
  }

  cv::Mat img2 = cv::imread(argv[2], 0);
  if (img2.empty())
  {
    std::cout << "Failed to read image 2 " << argv[2] << ". Stopping program." << std::endl;
    return -1;
  }

  // Setup VulkanSIFT
  vksift_setLogLevel(VKSIFT_LOG_INFO);

  if (vksift_loadVulkan() != VKSIFT_SUCCESS)
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  vksift_Config config = vksift_getDefaultConfig();
  config.input_image_max_size = std::max(img1.cols * img1.rows, img2.cols * img2.rows);

  vksift_Instance vksift_instance = NULL;
  if (vksift_createInstance(&vksift_instance, &config) != VKSIFT_SUCCESS)
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  // Keypoints for the image 1 and 2
  std::vector<vksift_Feature> img1_kp;
  std::vector<vksift_Feature> img2_kp;
  // Matches results (2-nearest neighbors information)
  std::vector<vksift_Match_2NN> matches_info12;
  std::vector<vksift_Match_2NN> matches_info21;
  // Vectors used to store the (filtered) keypoint matches in both images (same index=match)
  std::vector<vksift_Feature> matches_1;
  std::vector<vksift_Feature> matches_2;

  //////////////////////
  int user_key = 0;
  while (user_key != 'x')
  {
    // Detect SIFT features on both image (use two different GPU buffers to perform matching directly on the GPU)
    vksift_detectFeatures(vksift_instance, img1.data, img1.cols, img1.rows, 0u);
    vksift_detectFeatures(vksift_instance, img2.data, img2.cols, img2.rows, 1u);

    // For each SIFT feature in the buffer 0 (for image1), find the 2 nearest neighbors in the buffer 1 (image2)
    vksift_matchFeatures(vksift_instance, 0u, 1u);
    matches_info12.resize(vksift_getMatchesNumber(vksift_instance));
    // Matches must be downloaded before calling vksift_matchFeatures() another time, otherwise results are overwritten
    vksift_downloadMatches(vksift_instance, matches_info12.data());

    // Rerun the matching, but this time match the buffer 1 with the buffer 0
    vksift_matchFeatures(vksift_instance, 1u, 0u);
    matches_info21.resize(vksift_getMatchesNumber(vksift_instance));
    vksift_downloadMatches(vksift_instance, matches_info21.data());

    // Reserve enough space in the feature vectors to copy the SIFT features
    img1_kp.resize(vksift_getFeaturesNumber(vksift_instance, 0u));
    img2_kp.resize(vksift_getFeaturesNumber(vksift_instance, 1u));
    vksift_downloadFeatures(vksift_instance, img1_kp.data(), 0u);
    vksift_downloadFeatures(vksift_instance, img2_kp.data(), 1u);

    matches_1.clear();
    matches_2.clear();

    for (uint32_t i = 0; i < (uint32_t)matches_info12.size(); i++)
    {
      int idx_in_2 = matches_info12[i].idx_b1;
      // Check mutual match (an image 1 keypoint must also be the nearest neighbor of its nearest neighbor)
      if (matches_info21[idx_in_2].idx_b1 == i)
      {
        // Check Lowe's ratio in 1 (top1 NN and top2 NN distances must not be close, match must be discriminant enough)
        if ((matches_info12[i].dist_a_b1 / matches_info12[i].dist_a_b2) < 0.75)
        {
          // Check Lowe's ratio in 2
          if ((matches_info21[idx_in_2].dist_a_b1 / matches_info21[idx_in_2].dist_a_b2) < 0.75)
          {
            matches_1.push_back(img1_kp[matches_info12[i].idx_a]);
            matches_2.push_back(img2_kp[matches_info12[i].idx_b1]);
          }
        }
      }
    }
    std::cout << "Found " << matches_1.size() << " matches" << std::endl;

    // Draw keypoints for each image
    cv::Mat draw_frame1, draw_frame2;
    img1.convertTo(draw_frame1, CV_8UC3);
    img2.convertTo(draw_frame2, CV_8UC3);
    cv::cvtColor(draw_frame1, draw_frame1, cv::COLOR_GRAY2BGR);
    cv::cvtColor(draw_frame2, draw_frame2, cv::COLOR_GRAY2BGR);
    draw_frame1 = getOrientedKeypointsImage(img1.data, img1_kp, img1.cols, img1.rows);
    draw_frame2 = getOrientedKeypointsImage(img2.data, img2_kp, img2.cols, img2.rows);
    cv::imshow("VulkanSIFT image1 keypoints", draw_frame1);
    cv::imshow("VulkanSIFT image2 keypoints", draw_frame2);

    // Draw matches
    cv::Mat matches_image = getKeypointsMatchesImage(img1.data, matches_1, img1.cols, img1.rows, img2.data, matches_2, img2.cols, img2.rows);
    cv::putText(matches_image, "x: exit", cv::Size{10, matches_image.rows - 20}, cv::FONT_HERSHEY_COMPLEX, 0.5f, cv::Scalar(0, 255, 0));
    cv::imshow("VulkanSIFT matches", matches_image);

    user_key = cv::waitKey(1);
  }

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}