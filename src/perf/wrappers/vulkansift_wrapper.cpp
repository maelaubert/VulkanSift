#include "perf/wrappers/vulkansift_wrapper.h"

bool VulkanSiftDetector::init()
{
  if (!m_instance.init(640, 480, true, true))
  {
    return false;
  }
  // Can't init detector here, input resolution may change
  if (!m_matcher.init(&m_instance))
  {
    return false;
  }
  return true;
}
void VulkanSiftDetector::terminate()
{
  m_matcher.terminate();
  m_instance.terminate();
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
  VulkanSIFT::SiftDetector detector;
  std::vector<VulkanSIFT::SIFT_Feature> img1_kps, img2_kps;

  uint8_t *img1_buf = allocAndFillGreyBufferFromCvMat(image1);
  uint8_t *img2_buf = allocAndFillGreyBufferFromCvMat(image2);

  // Compute SIFT features on both images
  if (!detector.init(&m_instance, image1.cols, image1.rows))
  {
    delete[] img1_buf;
    delete[] img2_buf;
    return;
  }
  detector.compute(img1_buf, img1_kps);
  detector.terminate();

  if (!detector.init(&m_instance, image2.cols, image2.rows))
  {
    delete[] img1_buf;
    delete[] img2_buf;
    return;
  }
  detector.compute(img2_buf, img2_kps);
  detector.terminate();

  delete[] img1_buf;
  delete[] img2_buf;

  kps_img1.clear();
  kps_img2.clear();
  for (auto kp : img1_kps)
  {
    kps_img1.push_back({(float)kp.orig_x, (float)kp.orig_y});
  }
  for (auto kp : img2_kps)
  {
    kps_img2.push_back({(float)kp.orig_x, (float)kp.orig_y});
  }

  // Find feature matches
  std::vector<VulkanSIFT::SIFT_2NN_Info> matches_info;
  m_matcher.compute(img1_kps, img2_kps, matches_info);

  // Fill Match vector
  matches_img1.clear();
  matches_img2.clear();
  for (int i = 0; i < matches_info.size(); i++)
  {
    if ((matches_info[i].dist_ab1 / matches_info[i].dist_ab2) < LOWES_RATIO)
    {
      matches_img1.push_back({(float)img1_kps[matches_info[i].idx_a].orig_x, (float)img1_kps[matches_info[i].idx_a].orig_y});
      matches_img2.push_back({(float)img2_kps[matches_info[i].idx_b1].orig_x, (float)img2_kps[matches_info[i].idx_b1].orig_y});
    }
  }
}

float VulkanSiftDetector::measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter)
{
  VulkanSIFT::SiftDetector detector;
  if (!detector.init(&m_instance, image.cols, image.rows))
  {
    return -1.f;
  }

  uint8_t *img_buf = allocAndFillGreyBufferFromCvMat(image);

  std::vector<VulkanSIFT::SIFT_Feature> sift_kp;
  for (int i = 0; i < nb_warmup_iter; i++)
  {
    detector.compute(img_buf, sift_kp);
    std::cout << "\rWarmup " << i + 1 << "/" << nb_warmup_iter;
  }
  std::cout << std::endl;

  float sum_duration = 0;
  for (int i = 0; i < nb_iter; i++)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    detector.compute(img_buf, sift_kp);
    float duration =
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count()) / 1000.f;
    sum_duration += duration;
    std::cout << "\rMeasuring " << i + 1 << "/" << nb_iter;
  }
  std::cout << std::endl;
  float mean_duration = float(sum_duration) / float(nb_iter);

  delete[] img_buf;
  detector.terminate();

  return mean_duration;
}
