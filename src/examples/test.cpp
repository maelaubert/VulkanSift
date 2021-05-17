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

cv::Mat getColormappedDoGImage(cv::Mat image)
{
  cv::Mat out_image;
  image.copyTo(out_image);
  cv::cvtColor(out_image, out_image, cv::COLOR_GRAY2BGR);
  // out_image.create(image.rows, image.cols, CV_8UC3);
  for (int y = 0; y < image.rows; y++)
  {
    for (int x = 0; x < image.cols; x++)
    {
      float val = image.at<float>(cv::Point(x, y));
      if (val >= 0)
      {
        val = fminf(fabs(val) / 0.15f, 1.f);
        out_image.at<cv::Point3f>(cv::Point(x, y)) = cv::Point3f(0.f, val, 0.f);
      }
      else
      {
        val = fminf(fabs(val) / 0.15f, 1.f);
        out_image.at<cv::Point3f>(cv::Point(x, y)) = cv::Point3f(0.f, 0.f, val);
      }
    }
  }
  return out_image;
}

int main()
{
  cv::Mat grayimg = cv::imread("../res/img1.ppm", cv::ImreadModes::IMREAD_GRAYSCALE);
  cv::resize(grayimg, grayimg, cv::Size(1920, 1080));
  // cv::resize(grayimg, grayimg, cv::Size(640, 480));

  vksift_setLogLevel(VKSIFT_LOG_DEBUG);

  if (!vksift_loadVulkan())
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  vksift_Config config = vksift_Config_Default;
  // config.pyramid_precision_mode = VKSIFT_PYRAMID_PRECISION_FLOAT16;
  config.use_hardware_interpolated_blur = true;
  vksift_Instance vksift_instance = NULL;
  if (!vksift_createInstance(&vksift_instance, &config))
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  std::vector<vksift_Feature> feats;
  while (vksift_presentDebugFrame(vksift_instance))
  {
    auto start_ts = std::chrono::high_resolution_clock::now();
    vksift_detectFeatures(vksift_instance, grayimg.data, grayimg.cols, grayimg.rows, 0u);
    auto detect1_ts = std::chrono::high_resolution_clock::now();
    vksift_detectFeatures(vksift_instance, grayimg.data, grayimg.cols, grayimg.rows, 1u);
    auto detect2_ts = std::chrono::high_resolution_clock::now();

    uint32_t nb_sift = vksift_getFeaturesNumber(vksift_instance, 0u);
    std::cout << "Feature found: " << nb_sift << std::endl;
    feats.resize(nb_sift);
    vksift_downloadFeatures(vksift_instance, feats.data(), 0u);
    auto download1_ts = std::chrono::high_resolution_clock::now();

    vksift_uploadFeatures(vksift_instance, feats.data(), feats.size(), 0);
    auto upload1_ts = std::chrono::high_resolution_clock::now();

    cv::Mat frame = getOrientedKeypointsImage(grayimg.data, feats, grayimg.cols, grayimg.rows);
    cv::imshow("test", frame);
    cv::waitKey(1);

    vksift_downloadFeatures(vksift_instance, feats.data(), 1u);
    auto download2_ts = std::chrono::high_resolution_clock::now();

    vksift_uploadFeatures(vksift_instance, feats.data(), feats.size(), 1);
    auto upload2_ts = std::chrono::high_resolution_clock::now();

    std::cout << "Time to detect1: " << std::chrono::duration_cast<std::chrono::microseconds>(detect1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to detect2: " << std::chrono::duration_cast<std::chrono::microseconds>(detect2_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to download1: " << std::chrono::duration_cast<std::chrono::microseconds>(download1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to download2: " << std::chrono::duration_cast<std::chrono::microseconds>(download2_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to upload1: " << std::chrono::duration_cast<std::chrono::microseconds>(upload1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to upload2: " << std::chrono::duration_cast<std::chrono::microseconds>(upload2_ts - start_ts).count() / 1000.f << std::endl;

    vksift_matchFeatures(vksift_instance, 0u, 1u);
    uint32_t match_number = vksift_getMatchesNumber(vksift_instance);
    std::cout << "Matches found: " << match_number << std::endl;
    // buffer_idx = (buffer_idx + 1) % config.sift_buffer_count;
  }

  /*uint32_t nb_octave = vksift_getScaleSpaceNbOctaves(vksift_instance);
  for (uint32_t oct_idx = 0; oct_idx < nb_octave; oct_idx++)
  {
    for (uint32_t scale_idx = 0; scale_idx < config.nb_scales_per_octave + 3; scale_idx++)
    {
      cv::Mat blurred_image;
      uint32_t width, height;
      vksift_getScaleSpaceOctaveResolution(vksift_instance, oct_idx, &width, &height);
      std::cout << "width: " << width << " height: " << height << std::endl;
      blurred_image.create(height, width, CV_32F);
      vksift_downloadScaleSpaceImage(vksift_instance, oct_idx, scale_idx, (float *)blurred_image.data);
      std::string window_name{"Blurred " + std::to_string(oct_idx) + "/" + std::to_string(scale_idx)};
      cv::imshow(window_name, blurred_image);
    }
    for (uint32_t scale_idx = 0; scale_idx < config.nb_scales_per_octave + 2; scale_idx++)
    {
      cv::Mat dog_image;
      uint32_t width, height;
      vksift_getScaleSpaceOctaveResolution(vksift_instance, oct_idx, &width, &height);
      std::cout << "width: " << width << " height: " << height << std::endl;
      dog_image.create(height, width, CV_32F);
      vksift_downloadDoGImage(vksift_instance, oct_idx, scale_idx, (float *)dog_image.data);
      cv::Mat color_dog_image = getColormappedDoGImage(dog_image);
      std::string window_name{"DoG " + std::to_string(oct_idx) + "/" + std::to_string(scale_idx)};
      cv::imshow(window_name, color_dog_image);
    }
  }
  cv::waitKey(0);*/

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}