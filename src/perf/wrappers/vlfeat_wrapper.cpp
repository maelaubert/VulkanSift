#include "perf/wrappers/vlfeat_wrapper.h"

bool VLFeatDetector::init() { return true; }
void VLFeatDetector::terminate() {}

void VLFeatDetector::detectSIFT(cv::Mat image, std::vector<cv::KeyPoint> &keypoints, cv::Mat &descs, bool convert_and_copy_to_cv_format)
{
  std::vector<std::array<uint8_t, 128>> desc_vec;
  VlSiftFilt *vlsift = vl_sift_new(image.cols, image.rows, -1, 3, -1);
  bool first_oct = true;
  double angles[4];
  float flt_desc_arr[128];

  while (true)
  {
    // Process octave
    int err;
    if (first_oct)
    {
      err = vl_sift_process_first_octave(vlsift, (float *)image.data);
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
    const VlSiftKeypoint *vl_keypoints = vl_sift_get_keypoints(vlsift);
    int nb_kpts = vl_sift_get_nkeypoints(vlsift);
    for (int kpt_i = 0; kpt_i < nb_kpts; kpt_i++)
    {
      // Get orientations
      int nb_ori = vl_sift_calc_keypoint_orientations(vlsift, angles, vl_keypoints + kpt_i);
      for (int ori_i = 0; ori_i < nb_ori; ori_i++)
      {
        // Get descriptors
        vl_sift_calc_keypoint_descriptor(vlsift, flt_desc_arr, vl_keypoints + kpt_i, angles[ori_i]);
        if (convert_and_copy_to_cv_format)
        {
          keypoints.push_back(cv::KeyPoint{cv::Point2f{vl_keypoints[kpt_i].x, vl_keypoints[kpt_i].y}, vl_keypoints[kpt_i].sigma});
        }

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

  if (convert_and_copy_to_cv_format)
  {
    descs.create((int)desc_vec.size(), 128, CV_8U);
    for (int i = 0; i < (int)desc_vec.size(); i++)
    {
      for (int j = 0; j < 128; j++)
      {
        descs.at<uint8_t>(i, j) = desc_vec[i][j];
      }
    }
  }
}
