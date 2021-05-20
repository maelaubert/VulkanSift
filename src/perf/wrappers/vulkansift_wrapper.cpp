#include "perf/wrappers/vulkansift_wrapper.h"

bool VulkanSiftDetector::init()
{
  if (!vksift_loadVulkan())
  {
    return false;
  }
  vksift_Config config = vksift_getDefaultConfig();
  config.use_hardware_interpolated_blur = true;
  config.input_image_max_size = 1920 * 2 * 1080 * 2;
  sift_instance = NULL;
  if (!vksift_createInstance(&sift_instance, &config))
  {
    return false;
  }
  return true;
}
void VulkanSiftDetector::terminate()
{
  vksift_destroyInstance(&sift_instance);
  vksift_unloadVulkan();
}

void VulkanSiftDetector::detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format)
{
  std::vector<vksift_Feature> img_kps;

  // Compute SIFT features on both images
  vksift_detectFeatures(sift_instance, image.data, image.cols, image.rows, 0);

  img_kps.resize(vksift_getFeaturesNumber(sift_instance, 0));
  vksift_downloadFeatures(sift_instance, img_kps.data(), 0);

  if (convert_and_copy_to_cv_format)
  {
    keypoints.clear();
    descs.create((int)img_kps.size(), 128, CV_8U);

    for (int i = 0; i < (int)img_kps.size(); i++)
    {
      // Copy keypoint
      keypoints.push_back(cv::KeyPoint{cv::Point2f{img_kps[i].x, img_kps[i].y}, 0});
      // Copy descriptor
      for (int j = 0; j < 128; j++)
      {
        descs.at<uint8_t>(i, j) = img_kps[i].descriptor[j];
      }
    }
  }
}