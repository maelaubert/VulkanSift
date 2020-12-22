#include "perf/wrappers/siftgpu_wrapper.h"
#include "SiftGPU/src/SiftGPU.h"
#include "perf/wrappers/wrapper.h"
#include <GL/gl.h>
#include <vector>

bool SiftGPUDetector::init() { return true; }
void SiftGPUDetector::terminate() {}

uint8_t *SiftGPUDetector::allocAndFillGreyBufferFromCvMat(cv::Mat image)
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

void SiftGPUDetector::getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                                 std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2)
{
  SiftGPU sift;
  const char *argv[] = {"-fo", "-1", "-v", "0", "-cuda", "0"};
  sift.ParseParam(6, argv);
  int support = sift.CreateContextGL();
  if (support != SiftGPU::SIFTGPU_FULL_SUPPORTED)
  {
    std::cout << "SiftGPU not supported" << std::endl;
    return;
  }

  uint8_t *img1_buf = allocAndFillGreyBufferFromCvMat(image1);
  uint8_t *img2_buf = allocAndFillGreyBufferFromCvMat(image2);

  // Compute SIFT features on both images
  sift.RunSIFT(image1.cols, image1.rows, img1_buf, GL_LUMINANCE, GL_UNSIGNED_BYTE);
  int nb_sift_found1 = sift.GetFeatureNum();

  std::vector<float> descriptors1;
  descriptors1.resize(128 * nb_sift_found1);
  std::vector<SiftGPU::SiftKeypoint> keypoints1;
  keypoints1.resize(nb_sift_found1);
  sift.GetFeatureVector(&keypoints1[0], &descriptors1[0]);

  sift.RunSIFT(image2.cols, image2.rows, img2_buf, GL_LUMINANCE, GL_UNSIGNED_BYTE);
  int nb_sift_found2 = sift.GetFeatureNum();
  std::vector<float> descriptors2;
  descriptors2.resize(128 * nb_sift_found2);
  std::vector<SiftGPU::SiftKeypoint> keypoints2;
  keypoints2.resize(nb_sift_found2);
  sift.GetFeatureVector(&keypoints2[0], &descriptors2[0]);

  delete[] img1_buf;
  delete[] img2_buf;

  kps_img1.clear();
  kps_img2.clear();
  for (auto kp : keypoints1)
  {
    kps_img1.push_back({(float)kp.x, (float)kp.y});
  }
  for (auto kp : keypoints2)
  {
    kps_img2.push_back({(float)kp.x, (float)kp.y});
  }

  cv::Mat descs1, descs2;
  descs1.create(nb_sift_found1, 128, CV_8U);
  descs2.create(nb_sift_found2, 128, CV_8U);
  for (int i = 0; i < nb_sift_found1; i++)
  {
    for (int j = 0; j < 128; j++)
    {
      descs1.at<uint8_t>(i, j) = static_cast<uint8_t>(descriptors1[i * 128 + j] * 512.f);
    }
  }
  for (int i = 0; i < nb_sift_found2; i++)
  {
    for (int j = 0; j < 128; j++)
    {
      descs2.at<uint8_t>(i, j) = static_cast<uint8_t>(descriptors2[i * 128 + j] * 512.f);
    }
  }

  // Find feature matches
  std::vector<std::vector<cv::DMatch>> matches;
  auto matcher = cv::BFMatcher::create(cv::NORM_L2);
  matcher->knnMatch(descs1, descs2, matches, 2);

  // Fill Match vector
  matches_img1.clear();
  matches_img2.clear();
  for (int i = 0; i < matches.size(); i++)
  {
    if ((matches[i][0].distance / matches[i][1].distance) < LOWES_RATIO)
    {
      matches_img1.push_back(kps_img1[matches[i][0].queryIdx]);
      matches_img2.push_back(kps_img2[matches[i][0].trainIdx]);
    }
  }
}

float SiftGPUDetector::measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter)
{
  SiftGPU sift;
  const char *argv[] = {"-fo", "-1", "-v", "0", "-cuda", "0"};
  sift.ParseParam(6, argv);
  int support = sift.CreateContextGL();
  if (support != SiftGPU::SIFTGPU_FULL_SUPPORTED)
  {
    std::cout << "SiftGPU not supported" << std::endl;
    return 0.f;
  }

  uint8_t *img_buf = allocAndFillGreyBufferFromCvMat(image);
  std::vector<float> descriptors;
  std::vector<SiftGPU::SiftKeypoint> keypoints;

  for (int i = 0; i < nb_warmup_iter; i++)
  {
    sift.RunSIFT(image.cols, image.rows, img_buf, GL_LUMINANCE, GL_UNSIGNED_BYTE);
    int nb_sift_found1 = sift.GetFeatureNum();
    descriptors.resize(128 * nb_sift_found1);
    keypoints.resize(nb_sift_found1);
    sift.GetFeatureVector(keypoints.data(), descriptors.data());
    std::cout << "\rWarmup " << i + 1 << "/" << nb_warmup_iter;
  }
  std::cout << std::endl;

  float sum_duration = 0;
  for (int i = 0; i < nb_iter; i++)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    sift.RunSIFT(image.cols, image.rows, img_buf, GL_LUMINANCE, GL_UNSIGNED_BYTE);
    int nb_sift_found1 = sift.GetFeatureNum();
    descriptors.resize(128 * nb_sift_found1);
    keypoints.resize(nb_sift_found1);
    sift.GetFeatureVector(keypoints.data(), descriptors.data());
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
