extern "C"
{
#include "vulkansift/vulkansift.h"
}
#include "test_utils.h"

#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>

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
  // Calling vksift_presentDebugFrame() draw an empty to the empty window, every GPU commands executed between two frame
  // draw (what's inside the while loop) can be profiled/debug with GPU debugger (Nsight, Renderdoc, and probably other tools)
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

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}