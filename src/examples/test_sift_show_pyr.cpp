#include "test_utils.h"
#include <vulkansift/vulkansift.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

int main()
{
  vksift_setLogLevel(VKSIFT_LOG_INFO);

  if (!vksift_loadVulkan())
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  vksift_Config config = vksift_getDefaultConfig();
  vksift_Instance vksift_instance = NULL;
  if (!vksift_createInstance(&vksift_instance, &config))
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  // Read image with OpenCV
  cv::Mat image = cv::imread("res/img1.ppm", 0); // force loading as grey image to directly get a CV_8UC1 format

  // Run feature detection in which the pyramid will be created to extract keypoints
  vksift_detectFeatures(vksift_instance, image.data, image.cols, image.rows, 0u);

  uint32_t oct_idx = 0;
  uint32_t scale_idx = 0;
  uint32_t nb_octaves = vksift_getScaleSpaceNbOctaves(vksift_instance);
  uint32_t nb_scales = config.nb_scales_per_octave;
  int user_key = 0;

  cv::namedWindow("Pyramid viewer", cv::WINDOW_NORMAL);

  while (user_key != 'x')
  {
    uint32_t width, height;
    vksift_getScaleSpaceOctaveResolution(vksift_instance, oct_idx, &width, &height);
    // Get blurred image
    cv::Mat blurred_image;
    blurred_image.create(height, width, CV_32F);
    vksift_downloadScaleSpaceImage(vksift_instance, oct_idx, scale_idx, (float *)blurred_image.data);
    cv::cvtColor(blurred_image, blurred_image, cv::COLOR_GRAY2BGR);
    cv::resize(blurred_image, blurred_image, image.size());

    // Get next scale blurred image
    cv::Mat next_blurred_image;
    next_blurred_image.create(height, width, CV_32F);
    vksift_downloadScaleSpaceImage(vksift_instance, oct_idx, scale_idx + 1, (float *)next_blurred_image.data);
    cv::cvtColor(next_blurred_image, next_blurred_image, cv::COLOR_GRAY2BGR);
    cv::resize(next_blurred_image, next_blurred_image, image.size());

    // Get DoG image corresponding to the two blurred image substraction
    cv::Mat dog_image;
    dog_image.create(height, width, CV_32F);
    vksift_downloadDoGImage(vksift_instance, oct_idx, scale_idx, (float *)dog_image.data);
    cv::Mat color_dog_image = getColormappedDoGImage(dog_image);
    cv::resize(color_dog_image, color_dog_image, image.size());

    std::array<cv::Mat, 3> image_arr = {blurred_image, next_blurred_image, color_dog_image};
    cv::Mat final_image;
    cv::hconcat(image_arr, final_image);

    // Draw info
    cv::putText(final_image, "w/s: change octave", cv::Size{10, final_image.rows - 60}, cv::FONT_HERSHEY_COMPLEX, 0.5f, cv::Scalar(0, 0, 1));
    cv::putText(final_image, "d/a: change scale", cv::Size{10, final_image.rows - 40}, cv::FONT_HERSHEY_COMPLEX, 0.5f, cv::Scalar(0, 0, 1));
    cv::putText(final_image, "x: exit", cv::Size{10, final_image.rows - 20}, cv::FONT_HERSHEY_COMPLEX, 0.5f, cv::Scalar(0, 1, 0));

    std::string blurred_str{"Octave " + std::to_string(oct_idx) + " Scale " + std::to_string(scale_idx)};
    std::string next_blurred_str{"Scale " + std::to_string(scale_idx + 1)};
    std::string dog_str{"DoG"};
    cv::putText(final_image, blurred_str, cv::Size{10, 20}, cv::FONT_HERSHEY_COMPLEX, 0.5f, cv::Scalar(0, 0, 1));
    cv::putText(final_image, next_blurred_str, cv::Size{final_image.cols / 3 + 10, 20}, cv::FONT_HERSHEY_COMPLEX, 0.5f, cv::Scalar(0, 0, 1));
    cv::putText(final_image, dog_str, cv::Size{final_image.cols / 3 * 2 + 10, 20}, cv::FONT_HERSHEY_COMPLEX, 0.5f, cv::Scalar(0, 0, 1));

    cv::imshow("Pyramid viewer", final_image);
    user_key = cv::waitKey(0);
    // Handle user input
    switch (user_key)
    {
    case 'w': // up key
      oct_idx = (oct_idx != nb_octaves - 1) ? oct_idx + 1 : oct_idx;
      break;
    case 's': // down key
      oct_idx = (oct_idx != 0) ? oct_idx - 1 : oct_idx;
      break;
    case 'd': // right key
      scale_idx = (scale_idx != (nb_scales + 1)) ? scale_idx + 1 : scale_idx;
      break;
    case 'a': // left key
      scale_idx = (scale_idx != 0) ? scale_idx - 1 : scale_idx;
      break;
    default:
      break;
    }
  }

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}