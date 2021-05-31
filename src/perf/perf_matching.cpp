#include "perf/perf_common.h"
#include <fstream>
#include <iostream>

#define PIXEL_DIST_THRESHOLD 2.5f

void readHomographyInfoFile(const std::string &file_path, std::array<float, 9> &homography)
{
  std::ifstream info_file{file_path};
  std::string line_data;

  // The lines corresponds to the homography between image 1 and image 2..N
  // Each of the 3 lines contains 3 floating point numbers to represent the 3x3 matrix (row-major)
  for (int i = 0; i < 3; i++)
  {
    std::getline(info_file, line_data);
    std::istringstream iss = std::istringstream{line_data};
    iss >> homography[i * 3 + 0] >> homography[i * 3 + 1] >> homography[i * 3 + 2];
  }
  // Release file as we have every info we need
  info_file.close();

  for (int j = 0; j < 9; j++)
  {
    std::cout << homography[j] << " ";
  }
  std::cout << std::endl;
}

void computeMetrics(cv::Mat &img1, cv::Mat &img2, const std::vector<cv::KeyPoint> &kp_img1, const std::vector<cv::KeyPoint> &kp_img2,
                    const std::vector<CommonPoint> &matches_img1, const std::vector<CommonPoint> &matches_img2, std::array<float, 9> H1to2,
                    float &repeatability, float &putative_match_ratio, float &precision, float &matching_score)
{
  // Use OpenCV to compute repeatability score
  cv::Mat H_mat;
  H_mat.create(3, 3, CV_32F);
  H_mat.at<float>(0, 0) = H1to2[0];
  H_mat.at<float>(0, 1) = H1to2[1];
  H_mat.at<float>(0, 2) = H1to2[2];
  H_mat.at<float>(1, 0) = H1to2[3];
  H_mat.at<float>(1, 1) = H1to2[4];
  H_mat.at<float>(1, 2) = H1to2[5];
  H_mat.at<float>(2, 0) = H1to2[6];
  H_mat.at<float>(2, 1) = H1to2[7];
  H_mat.at<float>(2, 2) = H1to2[8];
  std::vector<cv::KeyPoint> kp1 = kp_img1;
  std::vector<cv::KeyPoint> kp2 = kp_img2;
  int nb_corresp;
  cv::evaluateFeatureDetector(img1, img2, H_mat, &kp1, &kp2, repeatability, nb_corresp);

  // Check number of valid matches (w.r.t homography)
  int cnt_inliers = 0;
  for (int i = 0; i < (int)matches_img1.size(); i++)
  {
    CommonPoint pt_in_img1 = matches_img1[i];
    CommonPoint pt_in_img2 = matches_img2[i];
    CommonPoint gt_in_img2;
    float w_inv = 1.f / ((H1to2[6] * pt_in_img1.x) + (H1to2[7] * pt_in_img1.y) + H1to2[8]);
    gt_in_img2.x = ((H1to2[0] * pt_in_img1.x) + (H1to2[1] * pt_in_img1.y) + H1to2[2]) * w_inv;
    gt_in_img2.y = ((H1to2[3] * pt_in_img1.x) + (H1to2[4] * pt_in_img1.y) + H1to2[5]) * w_inv;
    float dist = std::sqrt(std::pow(pt_in_img2.x - gt_in_img2.x, 2) + std::pow(pt_in_img2.y - gt_in_img2.y, 2));
    if (dist < PIXEL_DIST_THRESHOLD)
    {
      cnt_inliers++;
    }
  }

  // Putative match ratio = feats found / feats matched
  putative_match_ratio = (float)matches_img1.size() / (float)kp_img1.size();
  // Precision
  precision = (float)cnt_inliers / (float)matches_img1.size();
  // Precision
  matching_score = (float)cnt_inliers / (float)kp_img1.size();

  std::cout << "repeatability: " << repeatability << std::endl;
  std::cout << "putative_match_ratio: " << putative_match_ratio << std::endl;
  std::cout << "precision: " << precision << std::endl;
  std::cout << "matching_score: " << matching_score << std::endl;
}

void printUsage()
{
  std::cout << "Usage: ./perf_sift_match DATASET_PATH SIFT_DETECTOR_NAME" << std::endl;
  std::cout << "(for cross-detector matching you can use: ./perf_sift_match DATASET_PATH SIFT_DETECTOR_1_NAME SIFT_DETECTOR_2_NAME)" << std::endl;
  std::cout << "Available detector names: " << std::endl;
  for (auto det_name : getDetectorTypeNames())
  {
    std::cout << "\t " << det_name << std::endl;
  }
}

int main(int argc, char *argv[])
{
  //////////////////////////////////////////////////////////////////////////
  // Parameter handling to get name of requested SIFT detector/matcher
  if (argc != 3 && argc != 4)
  {
    std::cout << "Error: wrong number of arguments" << std::endl;
    printUsage();
    return -1;
  }

  std::string dataset_path{argv[1]};

  std::string detector_name{argv[2]};
  DETECTOR_TYPE detector_type;
  if (!getDetectorTypeFromName(detector_name, detector_type))
  {
    std::cout << "Error: invalid detector name" << std::endl;
    printUsage();
    return -1;
  }
  //////////////////////////////////////////////////////////////////////////
  // Initialize detector from its type
  std::cout << "Initializing " << detector_name << " detector..." << std::endl;
  std::shared_ptr<AbstractSiftDetector> detector1 = createDetector(detector_type);
  detector1->init();

  std::shared_ptr<AbstractSiftDetector> detector2 = detector1;

  bool with_second_detector = false;
  if (argc == 4 && detector_name != std::string{argv[3]})
  {
    std::string detector2_name{argv[3]};
    DETECTOR_TYPE detector2_type;
    if (!getDetectorTypeFromName(detector2_name, detector2_type))
    {
      std::cout << "Error: invalid name for second detector" << std::endl;
      printUsage();
      return -1;
    }
    std::cout << "Initializing " << detector_name << " detector..." << std::endl;
    detector2 = createDetector(detector2_type);
    detector2->init();
    with_second_detector = true;
  }

  // Prepare output file
  std::ofstream result_file{"matching_results_" + detector_name + ".txt"};

  //////////////////////////////////////////////////////////////////////////
  // Read Homography dataset
  std::vector<std::string> dataset_names_vec = {"bark", "bikes", "boat", "graf", "leuven", "trees", "ubc", "wall"};
  std::cout << dataset_names_vec.size() << std::endl;

  for (auto dataset_name : dataset_names_vec)
  {
    std::cout << "Dataset " << dataset_name << std::endl;
    std::string img_ext = ".ppm";
    if (dataset_name == "boat")
    {
      img_ext = ".pgm";
    }
    std::string img1_path = dataset_path + "/" + dataset_name + "/img1" + img_ext;
    cv::Mat img1 = cv::imread(img1_path, 0);
    if (img1.empty())
    {
      std::cout << "Failed to read image " << img1_path << std::endl;
      return 0;
    }
    if (detector1->useFloatImage())
    {
      img1.convertTo(img1, CV_32FC1);
    }

    std::vector<cv::KeyPoint> kp_img1;
    cv::Mat desc_img1;
    // Get features for image 1
    detector1->detectSIFT(img1, kp_img1, desc_img1, true);

    std::array<float, 9> homography;
    for (int n = 2; n <= 6; n++)
    {
      std::string homography_info_path = dataset_path + "/" + dataset_name + "/H1to" + std::to_string(n) + "p";
      readHomographyInfoFile(homography_info_path, homography);

      std::vector<cv::KeyPoint> kp_imgN;
      cv::Mat desc_imgN;

      std::string imgN_path = dataset_path + "/" + dataset_name + "/img" + std::to_string(n) + img_ext;
      cv::Mat imgN = cv::imread(imgN_path, 0);
      if (imgN.empty())
      {
        std::cout << "Failed to read image " << imgN_path << std::endl;
        return 0;
      }
      if (detector2->useFloatImage())
      {
        imgN.convertTo(imgN, CV_32FC1);
      }

      // Get features for image N
      detector2->detectSIFT(imgN, kp_imgN, desc_imgN, true);

      // Match features
      std::vector<CommonPoint> matches_img1, matches_img2;
      matchFeatures(img1, imgN, kp_img1, desc_img1, kp_imgN, desc_imgN, matches_img1, matches_img2, false);

      float repeatability, putative_match_ratio, precision, matching_score;

      // Get metrics
      computeMetrics(img1, imgN, kp_img1, kp_imgN, matches_img1, matches_img2, homography, repeatability, putative_match_ratio, precision, matching_score);
      // Write them to output file
      std::string res_str = dataset_name + ";" + std::to_string(1) + ";" + std::to_string(n + 1) + ";" + std::to_string(repeatability) + ";" +
                            std::to_string(putative_match_ratio) + ";" + std::to_string(precision) + ";" + std::to_string(matching_score) + "\n";
      result_file.write(res_str.c_str(), res_str.size());
    }
  }

  detector1->terminate();
  if (with_second_detector)
  {
    detector2->terminate();
  }
  result_file.close();

  return 0;
}