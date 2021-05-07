extern "C"
{
#include "vulkansift/vulkansift.h"
}

#include <cstring>
#include <iostream>

#include <opencv4/opencv2/opencv.hpp>

cv::Mat getOrientedKeypointsImage(uint8_t *in_img, std::vector<vksift_Feature> kps, int width, int height)
{

  cv::Mat output_mat(height, width, CV_8U);

  for (int i = 0; i < width * height; i++)
  {
    output_mat.data[i] = in_img[i];
  }

  cv::Mat output_mat_rgb(height, width, CV_8UC3);
  output_mat.convertTo(output_mat_rgb, CV_8UC3);

  cv::cvtColor(output_mat_rgb, output_mat_rgb, cv::COLOR_GRAY2BGR);

  srand(time(NULL));

  for (vksift_Feature kp : kps)
  {
    cv::Scalar color(rand() % 255, rand() % 255, rand() % 255, rand() % 255);

    int radius = kp.sigma;
    cv::circle(output_mat_rgb, cv::Point(kp.orig_x, kp.orig_y), radius, color, 1);
    float angle = kp.theta;
    if (angle > 3.14f)
    {
      angle -= 2.f * 3.14f;
    }

    cv::Point orient(cosf(angle) * radius, sinf(angle) * radius);
    cv::line(output_mat_rgb, cv::Point(kp.orig_x, kp.orig_y), cv::Point(kp.orig_x, kp.orig_y) + orient, color, 1);
  }

  return output_mat_rgb;
}

int main()
{
  cv::Mat grayimg = cv::imread("../res/img1.ppm", cv::ImreadModes::IMREAD_GRAYSCALE);
  cv::resize(grayimg, grayimg, cv::Size(1920, 1080));
  // cv::resize(grayimg, grayimg, cv::Size(640, 480));

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

  while (vksift_presentDebugFrame(vksift_instance))
  {
    auto start_ts = std::chrono::high_resolution_clock::now();
    vksift_detectFeatures(vksift_instance, grayimg.data, grayimg.cols, grayimg.rows, 0u);
    auto detect1_ts = std::chrono::high_resolution_clock::now();
    vksift_detectFeatures(vksift_instance, grayimg.data, grayimg.cols, grayimg.rows, 1u);
    auto detect2_ts = std::chrono::high_resolution_clock::now();

    uint32_t nb_sift = vksift_getFeaturesNumber(vksift_instance, 0u);
    std::cout << "Feature found: " << nb_sift << std::endl;
    std::vector<vksift_Feature> feats;
    feats.resize(nb_sift);
    vksift_downloadFeatures(vksift_instance, feats.data(), 0u);
    auto download1_ts = std::chrono::high_resolution_clock::now();

    cv::Mat frame = getOrientedKeypointsImage(grayimg.data, feats, grayimg.cols, grayimg.rows);
    cv::imshow("test", frame);
    cv::waitKey(1);

    vksift_downloadFeatures(vksift_instance, feats.data(), 1u);
    auto download2_ts = std::chrono::high_resolution_clock::now();

    std::cout << "Time to detect1: " << std::chrono::duration_cast<std::chrono::microseconds>(detect1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to detect2: " << std::chrono::duration_cast<std::chrono::microseconds>(detect2_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to download1: " << std::chrono::duration_cast<std::chrono::microseconds>(download1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to download2: " << std::chrono::duration_cast<std::chrono::microseconds>(download2_ts - start_ts).count() / 1000.f << std::endl;

    // buffer_idx = (buffer_idx + 1) % config.sift_buffer_count;
  }

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}