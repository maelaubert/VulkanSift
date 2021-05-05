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

  uint8_t *data = new uint8_t[1940 * 1090];

  while (vksift_presentDebugFrame(vksift_instance))
  {
    vksift_detectFeatures(vksift_instance, grayimg.data, grayimg.cols, grayimg.rows, 0);
  }

  /*std::cout << "Input 640 480" << std::endl;
  vksift_detectFeatures(vksift_instance, data, 640, 480, 0);

  std::cout << "Input 1920 1080" << std::endl;
  vksift_detectFeatures(vksift_instance, data, 1920, 1080, 0);

  std::cout << "Input 1918 1079" << std::endl;
  vksift_detectFeatures(vksift_instance, data, 1918, 1079, 0);
  std::cout << "Input 1918 1079" << std::endl;
  vksift_detectFeatures(vksift_instance, data, 1918, 1079, 0);*/

  delete[] data;

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}