#include "perf/wrappers/popsift_wrapper.h"
#include "perf/wrappers/wrapper.h"
#include "popsift/src/features.h"
#include "popsift/src/popsift.h"
#include <GL/gl.h>
#include <vector>

bool PopSiftDetector::init() { return true; }
void PopSiftDetector::terminate() {}

uint8_t *PopSiftDetector::allocAndFillGreyBufferFromCvMat(cv::Mat image)
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

void PopSiftDetector::getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                                 std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2)
{

  popsift::Config popsift_conf{};
  popsift_conf.setNormMode(popsift::Config::NormMode::Classic);
  PopSift detector{popsift_conf, popsift::Config::ProcessingMode::ExtractingMode, PopSift::ImageMode::ByteImages};

  uint8_t *img1_buf = allocAndFillGreyBufferFromCvMat(image1);
  uint8_t *img2_buf = allocAndFillGreyBufferFromCvMat(image2);

  // Compute SIFT features on both images
  SiftJob *job1 = detector.enqueue(image1.cols, image1.rows, img1_buf);
  popsift::Features *res1 = job1->get();
  SiftJob *job2 = detector.enqueue(image2.cols, image2.rows, img2_buf);
  popsift::Features *res2 = job2->get();

  delete[] img1_buf;
  delete[] img2_buf;

  kps_img1.clear();
  int nb_feat_1 = 0;
  int nb_feat_2 = 0;
  for (int i = 0; i < res1->getFeatureCount(); i++)
  {
    for (int j = 0; j < res1->getFeatures()[i].num_ori; j++)
    {
      kps_img1.push_back({res1->getFeatures()[i].xpos, res1->getFeatures()[i].ypos});
      nb_feat_1++;
    }
  }
  for (int i = 0; i < res2->getFeatureCount(); i++)
  {
    for (int j = 0; j < res2->getFeatures()[i].num_ori; j++)
    {
      kps_img2.push_back({res2->getFeatures()[i].xpos, res2->getFeatures()[i].ypos});
      nb_feat_2++;
    }
  }

  cv::Mat descs1, descs2;
  descs1.create(nb_feat_1, 128, CV_8U);
  descs2.create(nb_feat_2, 128, CV_8U);
  int cnt = 0;
  for (int i = 0; i < res1->getFeatureCount(); i++)
  {
    for (int j = 0; j < res1->getFeatures()[i].num_ori; j++)
    {
      for (int k = 0; k < 128; k++)
      {
        descs1.at<uint8_t>(cnt, k) = static_cast<uint8_t>(res1->getFeatures()[i].desc[j]->features[k] * 512.f);
      }
      cnt++;
    }
  }
  cnt = 0;
  for (int i = 0; i < res2->getFeatureCount(); i++)
  {
    for (int j = 0; j < res2->getFeatures()[i].num_ori; j++)
    {
      for (int k = 0; k < 128; k++)
      {
        descs2.at<uint8_t>(cnt, k) = static_cast<uint8_t>(res2->getFeatures()[i].desc[j]->features[k] * 512.f);
      }
      cnt++;
    }
  }

  delete job1;
  delete job2;
  delete res1;
  delete res2;

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
  }
}

float PopSiftDetector::measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter)
{
  popsift::Config popsift_conf{};
  popsift_conf.setNormMode(popsift::Config::NormMode::Classic);
  popsift_conf.setGaussMode(popsift::Config::GaussMode::VLFeat_Relative);
  PopSift detector{popsift_conf, popsift::Config::ProcessingMode::ExtractingMode, PopSift::ImageMode::ByteImages};

  uint8_t *img_buf = allocAndFillGreyBufferFromCvMat(image);
  std::vector<float> descriptors;

  for (int i = 0; i < nb_warmup_iter; i++)
  {
    SiftJob *job = detector.enqueue(image.cols, image.rows, img_buf);
    popsift::Features *res = job->get();
    delete job;
    delete res;
    std::cout << "\rWarmup " << i + 1 << "/" << nb_warmup_iter;
  }
  std::cout << std::endl;

  float sum_duration = 0;
  for (int i = 0; i < nb_iter; i++)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    SiftJob *job = detector.enqueue(image.cols, image.rows, img_buf);
    popsift::Features *res = job->get();
    delete job;
    delete res;
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
