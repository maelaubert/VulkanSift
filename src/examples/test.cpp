extern "C"
{
#include "vulkansift/vulkansift.h"
}

#include <cstring>
#include <iostream>

#include <opencv4/opencv2/opencv.hpp>

int main()
{
  cv::Mat grayimg = cv::imread("../res/img1.ppm", cv::ImreadModes::IMREAD_GRAYSCALE);
  cv::imshow("test", grayimg);
  cv::waitKey(0);

  vksift_setLogLevel(VKSIFT_LOG_INFO);

  if (!vksift_loadVulkan())
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  vksift_Config config = vksift_Config_Default;
  config.use_hardware_interpolated_blur = true;
  vksift_Instance vksift_instance = NULL;
  if (!vksift_createInstance(&vksift_instance, &config))
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  int buffer_idx = 0;
  while (vksift_presentDebugFrame(vksift_instance))
  {
    vksift_detectFeatures(vksift_instance, grayimg.data, grayimg.cols, grayimg.rows, buffer_idx);
    // buffer_idx = (buffer_idx + 1) % config.sift_buffer_count;
  }

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}