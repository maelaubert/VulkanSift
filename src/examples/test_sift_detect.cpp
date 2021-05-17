extern "C"
{
#include <vulkansift/vulkansift.h>
}
#include "test_utils.h"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

int main()
{

  vksift_setLogLevel(VKSIFT_LOG_INFO);

  // Load the Vulkan API (should never be called more than once per program)
  if (!vksift_loadVulkan())
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  // Create a vksift instance using the default configuration
  vksift_Config config = vksift_Config_Default;
  vksift_Instance vksift_instance = NULL;
  if (!vksift_createInstance(&vksift_instance, &config))
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  // Read image with OpenCV
  cv::Mat image = cv::imread("res/img1.ppm", 0);
  cv::resize(image, image, cv::Size(640, 480));

  std::vector<vksift_Feature> feat_vec;
  bool draw_oriented_keypoints = true;

  int user_key = 0;
  while (user_key != 'q')
  {
    // Run SIFT feature detection and copy the results to the CPU
    vksift_detectFeatures(vksift_instance, image.data, image.cols, image.rows, 0u);
    feat_vec.resize(vksift_getFeaturesNumber(vksift_instance, 0u));
    vksift_downloadFeatures(vksift_instance, feat_vec.data(), 0u);

    std::cout << "Feature found: " << feat_vec.size() << std::endl;

    cv::Mat draw_frame;
    image.convertTo(draw_frame, CV_8UC3);
    cv::cvtColor(draw_frame, draw_frame, cv::COLOR_GRAY2BGR);
    if (draw_oriented_keypoints)
    {
      draw_frame = getOrientedKeypointsImage(image.data, feat_vec, image.cols, image.rows);
    }
    else
    {
      // Draw only points at the SIFT position
      for (int i = 0; i < feat_vec.size(); i++)
      {
        cv::circle(draw_frame, cv::Point(feat_vec[i].orig_x, feat_vec[i].orig_y), 3, cv::Scalar(0, 0, 255), 1);
      }
    }

    cv::imshow("VulkanSIFT keypoints", draw_frame);
    user_key = cv::waitKey(1);
  }

  // Release vksift instance and API
  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}