#include "perf/wrappers/vlfeat_wrapper.h"

bool VLFeatDetector::init() { return true; }
void VLFeatDetector::terminate() {}

float *VLFeatDetector::allocAndFillGreyBufferFromCvMat(cv::Mat image)
{
  int width = image.cols;
  int height = image.rows;
  float *image_buf = new float[width * height];
  cv::Mat im_tmp;
  cv::Mat img_grey(cv::Size(width, height), CV_8UC1);
  cv::cvtColor(image, im_tmp, cv::COLOR_BGR2GRAY);
  im_tmp.convertTo(img_grey, CV_8UC1);
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      image_buf[j * width + i] = static_cast<float>(img_grey.at<uint8_t>(cv::Point(i, j))) / 255.f;
    }
  }

  return image_buf;
}

void detectVLFeat_SIFT(float *img_buf, int width, int height, std::vector<CommonPoint> &kpts, cv::Mat &descs)
{
  kpts.clear();
  std::vector<std::array<uint8_t, 128>> desc_vec;
  VlSiftFilt *vlsift = vl_sift_new(width, height, -1, 3, -1);
  bool first_oct = true;
  double angles[4];
  float flt_desc_arr[128];

  while (true)
  {
    // Process octave
    int err;
    if (first_oct)
    {
      err = vl_sift_process_first_octave(vlsift, img_buf);
      first_oct = false;
    }
    else
    {
      err = vl_sift_process_next_octave(vlsift);
    }
    if (err)
    {
      break;
    }

    // Extract keypoints from octave
    vl_sift_detect(vlsift);
    const VlSiftKeypoint *keypoints = vl_sift_get_keypoints(vlsift);
    int nb_kpts = vl_sift_get_nkeypoints(vlsift);
    for (int kpt_i = 0; kpt_i < nb_kpts; kpt_i++)
    {
      // Get orientations
      int nb_ori = vl_sift_calc_keypoint_orientations(vlsift, angles, keypoints + kpt_i);
      for (int ori_i = 0; ori_i < nb_ori; ori_i++)
      {
        // Get descriptors
        vl_sift_calc_keypoint_descriptor(vlsift, flt_desc_arr, keypoints + kpt_i, angles[ori_i]);
        kpts.push_back({keypoints[kpt_i].x, keypoints[kpt_i].y});
        std::array<uint8_t, 128> desc;
        for (int i = 0; i < 128; i++)
        {
          desc[i] = static_cast<uint8_t>(flt_desc_arr[i] * 512.f);
        }
        desc_vec.push_back(desc);
      }
    }
  }
  vl_sift_delete(vlsift);

  descs.create((int)desc_vec.size(), 128, CV_8U);
  for (int i = 0; i < desc_vec.size(); i++)
  {
    for (int j = 0; j < 128; j++)
    {
      descs.at<uint8_t>(i, j) = desc_vec[i][j];
    }
  }
}

void VLFeatDetector::getMatches(cv::Mat image1, cv::Mat image2, std::vector<CommonPoint> &kps_img1, std::vector<CommonPoint> &kps_img2,
                                std::vector<CommonPoint> &matches_img1, std::vector<CommonPoint> &matches_img2)
{
  float *img1_buf = allocAndFillGreyBufferFromCvMat(image1);
  float *img2_buf = allocAndFillGreyBufferFromCvMat(image2);

  cv::Mat descs1, descs2;
  detectVLFeat_SIFT(img1_buf, image1.cols, image1.rows, kps_img1, descs1);
  detectVLFeat_SIFT(img2_buf, image2.cols, image2.rows, kps_img2, descs2);

  delete[] img1_buf;
  delete[] img2_buf;

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

float VLFeatDetector::measureMeanExecutionTimeMs(cv::Mat image, int nb_warmup_iter, int nb_iter)
{
  std::vector<CommonPoint> kpts;
  cv::Mat descs;
  float *img_buf = allocAndFillGreyBufferFromCvMat(image);

  for (int i = 0; i < nb_warmup_iter; i++)
  {
    detectVLFeat_SIFT(img_buf, image.cols, image.rows, kpts, descs);
    std::cout << "\rWarmup " << i + 1 << "/" << nb_warmup_iter;
  }
  std::cout << std::endl;

  float sum_duration = 0;
  for (int i = 0; i < nb_iter; i++)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    detectVLFeat_SIFT(img_buf, image.cols, image.rows, kpts, descs);
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
