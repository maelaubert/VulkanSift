#include "perf/wrappers/popsift_wrapper.h"
#include "perf/wrappers/wrapper.h"
#include "popsift/src/features.h"
#include <GL/gl.h>
#include <vector>

bool PopSiftDetector::init()
{
  popsift::Config popsift_conf{};
  popsift_conf.setNormMode(popsift::Config::NormMode::Classic);
  detector = std::make_shared<PopSift>(popsift_conf, popsift::Config::ProcessingMode::ExtractingMode, PopSift::ImageMode::ByteImages);

  return true;
}
void PopSiftDetector::terminate() {}

void PopSiftDetector::detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format)
{

  // Compute SIFT features on both images
  SiftJob *job1 = detector->enqueue(image.cols, image.rows, image.data);
  popsift::Features *res1 = job1->get();
  int nb_feat = res1->getFeatureCount();

  if (convert_and_copy_to_cv_format)
  {
    keypoints.clear();
    int nb_feat_1 = 0;
    for (int i = 0; i < nb_feat; i++)
    {
      for (int j = 0; j < res1->getFeatures()[i].num_ori; j++)
      {
        keypoints.push_back(cv::KeyPoint{cv::Point2f{res1->getFeatures()[i].xpos, res1->getFeatures()[i].ypos}, res1->getFeatures()[i].sigma});
        nb_feat_1++;
      }
    }

    cv::Mat descs1, descs2;
    descs.create(nb_feat_1, 128, CV_8U);
    int cnt = 0;
    for (int i = 0; i < res1->getFeatureCount(); i++)
    {
      for (int j = 0; j < res1->getFeatures()[i].num_ori; j++)
      {
        for (int k = 0; k < 128; k++)
        {
          descs.at<uint8_t>(cnt, k) = static_cast<uint8_t>(res1->getFeatures()[i].desc[j]->features[k] * 512.f);
        }
        cnt++;
      }
    }
  }
  else
  {
    keypoints.resize(nb_feat);
  }

  delete job1;
  delete res1;
}