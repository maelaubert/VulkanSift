#include "perf/wrappers/siftgpu_wrapper.h"
#include "perf/wrappers/wrapper.h"
#include <GL/gl.h>
#include <vector>

bool SiftGPUDetector::init()
{
  const char *argv[] = {"-fo", "-1", "-v", "0", "-cuda", "0"};
  sift.ParseParam(6, argv);
  int support = sift.CreateContextGL();
  if (support != SiftGPU::SIFTGPU_FULL_SUPPORTED)
  {
    std::cout << "SiftGPU not supported" << std::endl;
    return false;
  }
  return true;
}
void SiftGPUDetector::terminate() {}

void SiftGPUDetector::detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format)
{
  // Compute SIFT features on both images
  sift.RunSIFT(image.cols, image.rows, image.data, GL_LUMINANCE, GL_UNSIGNED_BYTE);
  int nb_sift_found1 = sift.GetFeatureNum();

  std::vector<float> descriptors1;
  descriptors1.resize(128 * nb_sift_found1);
  std::vector<SiftGPU::SiftKeypoint> keypoints1;
  keypoints1.resize(nb_sift_found1);
  sift.GetFeatureVector(&keypoints1[0], &descriptors1[0]);

  if (convert_and_copy_to_cv_format)
  {
    keypoints.clear();
    for (int i = 0; i < nb_sift_found1; i++)
    {
      keypoints.push_back(cv::KeyPoint{cv::Point2f{(float)keypoints1[i].x, (float)keypoints1[i].y}, keypoints1[i].s});
    }

    descs.create(nb_sift_found1, 128, CV_8U);
    for (int i = 0; i < nb_sift_found1; i++)
    {
      for (int j = 0; j < 128; j++)
      {
        descs.at<uint8_t>(i, j) = static_cast<uint8_t>(descriptors1[i * 128 + j] * 512.f);
      }
    }
  }
  else
  {
    keypoints.resize(keypoints1.size());
  }
}