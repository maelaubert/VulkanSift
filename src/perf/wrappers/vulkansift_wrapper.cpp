#include "perf/wrappers/vulkansift_wrapper.h"

bool VulkanSiftDetector::init()
{
  if (!vksift_loadVulkan())
  {
    return false;
  }
  vksift_Config config = vksift_Config_Default;
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

uint8_t *VulkanSiftDetector::allocAndFillGreyBufferFromCvMat(cv::Mat image)
{
  int width = image.cols;
  int height = image.rows;
  uint8_t *image_buf = new uint8_t[width * height];
  cv::Mat im_tmp;
  cv::Mat img_grey(cv::Size(width, height), CV_8UC1);
  cv::cvtColor(image, im_tmp, cv::COLOR_BGR2GRAY);
  im_tmp.convertTo(img_grey, CV_8UC1);
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      image_buf[j * width + i] = img_grey.at<uint8_t>(cv::Point(i, j));
    }
  }

  return image_buf;
}

void VulkanSiftDetector::getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                                    std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2)
{
  std::vector<vksift_Feature> img1_kps, img2_kps;

  uint8_t *img1_buf = allocAndFillGreyBufferFromCvMat(image1);
  uint8_t *img2_buf = allocAndFillGreyBufferFromCvMat(image2);

  // Compute SIFT features on both images
  vksift_detectFeatures(sift_instance, img1_buf, image1.cols, image1.rows, 0);
  vksift_detectFeatures(sift_instance, img2_buf, image2.cols, image2.rows, 1);

  img1_kps.resize(vksift_getFeaturesNumber(sift_instance, 0));
  vksift_downloadFeatures(sift_instance, img1_kps.data(), 0);

  img2_kps.resize(vksift_getFeaturesNumber(sift_instance, 1));
  vksift_downloadFeatures(sift_instance, img2_kps.data(), 1);

  delete[] img1_buf;
  delete[] img2_buf;

  kps_img1.clear();
  kps_img2.clear();
  for (auto kp : img1_kps)
  {
    kps_img1.push_back({kp.x * kp.scale_factor, kp.y * kp.scale_factor});
  }
  for (auto kp : img2_kps)
  {
    kps_img2.push_back({kp.x * kp.scale_factor, kp.y * kp.scale_factor});
  }

  std::cout << kps_img1.size() << std::endl;
  std::cout << kps_img2.size() << std::endl;

  // Match using OpenCV matcher
  /*cv::Mat descs1, descs2;
  // Fill desc with vksift results
  descs1.create((int)kps_img1.size(), 128, CV_8U);
  for (int i = 0; i < kps_img1.size(); i++)
  {
    for (int j = 0; j < 128; j++)
    {
      descs1.at<uint8_t>(i, j) = img1_kps[i].feature_vector[j];
    }
  }
  descs2.create((int)kps_img2.size(), 128, CV_8U);
  for (int i = 0; i < kps_img2.size(); i++)
  {
    for (int j = 0; j < 128; j++)
    {
      descs2.at<uint8_t>(i, j) = img2_kps[i].feature_vector[j];
    }
  }

  // Find feature matches
  std::vector<std::vector<cv::DMatch>> matches12, matches21;
  auto matcher = cv::BFMatcher::create(cv::NORM_L2);
  matcher->knnMatch(descs1, descs2, matches12, 2);
  matcher->knnMatch(descs2, descs1, matches21, 2);

  // Fill Match vector
  matches_img1.clear();
  matches_img2.clear();
  for (int i = 0; i < matches12.size(); i++)
  {
    int idx_in_2 = matches12[i][0].trainIdx;
    // Check mutual match
    if (matches21[idx_in_2][0].trainIdx == i)
    {
      // Check Lowe's ratio in 1
      if ((matches12[i][0].distance / matches12[i][1].distance) < LOWES_RATIO)
      {
        // Check Lowe's ratio in 2
        if ((matches21[idx_in_2][0].distance / matches21[idx_in_2][1].distance) < LOWES_RATIO)
        {
          matches_img1.push_back(kps_img1[matches12[i][0].queryIdx]);
          matches_img2.push_back(kps_img2[matches12[i][0].trainIdx]);
        }
      }
    }
  }*/

  // Find feature matches
  std::vector<vksift_Match_2NN> matches_info12, matches_info21;
  vksift_matchFeatures(sift_instance, 0u, 1u);
  matches_info12.resize(vksift_getMatchesNumber(sift_instance));
  vksift_downloadMatches(sift_instance, matches_info12.data());

  vksift_matchFeatures(sift_instance, 1u, 0u);
  matches_info21.resize(vksift_getMatchesNumber(sift_instance));
  vksift_downloadMatches(sift_instance, matches_info21.data());

  // Fill Match vector
  matches_img1.clear();
  matches_img2.clear();
  for (int i = 0; i < matches_info12.size(); i++)
  {
    int idx_in_2 = matches_info12[i].idx_b1;
    // Check mutual match
    if (matches_info21[idx_in_2].idx_b1 == i)
    {
      // Check Lowe's ratio in 1
      if ((matches_info12[i].dist_a_b1 / matches_info12[i].dist_a_b2) < LOWES_RATIO)
      {
        // Check Lowe's ratio in 2
        if ((matches_info21[idx_in_2].dist_a_b1 / matches_info21[idx_in_2].dist_a_b2) < LOWES_RATIO)
        {
          matches_img1.push_back(kps_img1[matches_info12[i].idx_a]);
          matches_img2.push_back(kps_img2[matches_info12[i].idx_b1]);
        }
      }
    }
  }
}

float VulkanSiftDetector::measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter)
{
  uint8_t *img_buf = allocAndFillGreyBufferFromCvMat(image);

  std::vector<vksift_Feature> sift_kp;
  for (int i = 0; i < nb_warmup_iter; i++)
  {
    vksift_detectFeatures(sift_instance, img_buf, image.cols, image.rows, 0);
    uint32_t nb_sift_found = vksift_getFeaturesNumber(sift_instance, 0);
    sift_kp.resize(nb_sift_found);
    vksift_downloadFeatures(sift_instance, sift_kp.data(), 0);
    std::cout << "\rWarmup " << i + 1 << "/" << nb_warmup_iter;
  }
  std::cout << std::endl;

  float sum_duration = 0;
  for (int i = 0; i < nb_iter; i++)
  {
    auto start_time = std::chrono::high_resolution_clock::now();

    vksift_detectFeatures(sift_instance, img_buf, image.cols, image.rows, 0);
    uint32_t nb_sift_found = vksift_getFeaturesNumber(sift_instance, 0);
    sift_kp.resize(nb_sift_found);
    vksift_downloadFeatures(sift_instance, sift_kp.data(), 0);

    float duration =
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count()) / 1000.f;
    sum_duration += duration;
    std::cout << "\rMeasuring " << i + 1 << "/" << nb_iter;
  }
  std::cout << std::endl;
  float mean_duration = float(sum_duration) / float(nb_iter);

  delete[] img_buf;

  return mean_duration;
}
