#include "vulkansift/viz/vulkan_viewer.h"
#include "vulkansift/vulkansift.h"

#include <iostream>
#include <opencv2/opencv.hpp>

int main()
{

  VulkanInstance instance;
  if (!instance.init(640, 480, true, true))
  {
    std::cout << "Impossible to initialize VulkanInstance" << std::endl;
    return -1;
  }

  VulkanViewer viewer;
  if (!viewer.init(&instance, 640, 480))
  {
    std::cout << "Impossible to initialize VulkanViewer" << std::endl;
    return -1;
  }
  VulkanSIFT::SiftDetector detector;
  if (!detector.init(&instance, 640, 480))
  {
    std::cout << "Impossible to initialize SiftDetector" << std::endl;
    return -1;
  }

  // Read image with OpenCV
  cv::Mat img1 = cv::imread("res/img1.ppm");
  cv::resize(img1, img1, cv::Size(640, 480));

  int width1 = img1.cols;
  int height1 = img1.rows;
  uint8_t *image1 = new uint8_t[img1.rows * img1.cols];
  cv::Mat img1_grey(cv::Size(width1, height1), CV_8UC1);
  cv::cvtColor(img1, img1, cv::COLOR_BGR2GRAY);
  img1.convertTo(img1_grey, CV_8UC1);
  for (int i = 0; i < img1_grey.cols; i++)
  {
    for (int j = 0; j < img1_grey.rows; j++)
    {
      image1[j * img1_grey.cols + i] = img1_grey.at<uint8_t>(cv::Point(i, j));
    }
  }

  while (!viewer.shouldStop())
  {
    std::vector<VulkanSIFT::SIFT_Feature> feat_vec;
    detector.compute(image1, feat_vec);

    cv::Mat draw_frame;
    img1_grey.convertTo(draw_frame, CV_8UC3);
    cv::cvtColor(draw_frame, draw_frame, cv::COLOR_GRAY2BGR);
    // cv::copyTo(cv_frame_color, draw_frame, cv::Mat());
    for (int i = 0; i < feat_vec.size(); i++)
    {
      cv::circle(draw_frame, cv::Point(feat_vec[i].orig_x, feat_vec[i].orig_y), 3, cv::Scalar(0, 0, 255), 1);
    }
    cv::imshow("Test", draw_frame);
    cv::waitKey(30);

    float gpu_time;
    viewer.execOnce(image1, &gpu_time);
  }

  delete[] image1;

  detector.terminate();
  viewer.terminate();
  instance.terminate();

  return 0;
}