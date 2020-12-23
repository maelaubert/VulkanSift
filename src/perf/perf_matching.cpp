#include "perf/perf_common.h"
#include <fstream>
#include <iostream>

#define PIXEL_DIST_THRESHOLD 2.5f

cv::Mat getKeypointsMatchesImage(cv::Mat &in_img1, cv::Mat &in_img2, std::vector<CommonPoint> kps1, std::vector<CommonPoint> kps2)
{
  // Convert image 1
  cv::Mat output_mat_rgb1 = in_img1;

  // Convert image 2
  cv::Mat output_mat_rgb2 = in_img2;

  // Create concatenated image (update images to have the same size)
  int max_width = std::max(in_img1.cols, in_img2.cols);
  int max_height = std::max(in_img1.rows, in_img2.rows);
  cv::copyMakeBorder(output_mat_rgb1, output_mat_rgb1, 0, max_height - in_img1.rows, 0, max_width - in_img1.cols, cv::BORDER_CONSTANT,
                     cv::Scalar(0, 0, 0));
  cv::copyMakeBorder(output_mat_rgb2, output_mat_rgb2, 0, max_height - in_img2.rows, 0, max_width - in_img2.cols, cv::BORDER_CONSTANT,
                     cv::Scalar(0, 0, 0));

  cv::Mat concatenated_mat;
  cv::hconcat(output_mat_rgb1, output_mat_rgb2, concatenated_mat);

  srand(time(NULL));

  for (std::size_t i = 0; i < kps1.size(); i++)
  {
    cv::Scalar color(rand() % 255, rand() % 255, rand() % 255, rand() % 255);

    // output_mat.at<cv::Vec3b>(cv::Point(kp.orig_x, kp.orig_y)) = cv::Vec3b(0,
    // 0, 254); cv::circle(output_mat_rgb, cv::Point(kp.orig_x, kp.orig_y), 3,
    // cv::Scalar(0.0, 0.0, 255.0), 1);
    cv::Point im1_p(kps1.at(i).x, kps1.at(i).y);
    cv::Point im2_origin(max_width, 0);
    cv::Point im2_p(kps2.at(i).x, kps2.at(i).y);
    cv::line(concatenated_mat, im1_p, im2_origin + im2_p, color, 2, cv::LINE_AA);
  }

  return concatenated_mat;
}

void computeMetrics(const std::vector<CommonPoint> &kp_img1, const std::vector<CommonPoint> &kp_img2, const std::vector<CommonPoint> &matches_img1,
                    const std::vector<CommonPoint> &matches_img2, std::array<float, 9> H1to2, float &putative_match_ratio, float &precision,
                    float &matching_score, float &recall)
{
  // Check number of valid matches (w.r.t homography)
  int cnt_inliers = 0;
  for (int i = 0; i < matches_img1.size(); i++)
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
  // Recall (TODO)
  recall = 0.f;

  std::cout << "putative_match_ratio: " << putative_match_ratio << std::endl;
  std::cout << "precision: " << precision << std::endl;
  std::cout << "matching_score: " << matching_score << std::endl;
}

void printUsage()
{
  std::cout << "Usage: ./perf_sift_match SIFT_DETECTOR_NAME" << std::endl;
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
  if (argc != 2)
  {
    std::cout << "Error: wrong number of arguments" << std::endl;
    printUsage();
    return -1;
  }

  std::string detector_name{argv[1]};
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
  std::shared_ptr<AbstractSiftDetector> detector = createDetector(detector_type);
  detector->init();

  // Prepare output file
  std::ofstream result_file{"matching_results_" + detector_name + ".txt"};

  //////////////////////////////////////////////////////////////////////////
  // Read Homography dataset
  std::string path_root = "res/feature_evaluation_data/homography/";
  std::vector<std::string> dataset_names_vec = {"bark", "bikes",  "boat",  "ceiling", "day_night", "graffiti", "leuven",
                                                "rome", "semper", "trees", "ubc",     "venice",    "wall"};
  std::cout << dataset_names_vec.size() << std::endl;

  for (auto dataset_name : dataset_names_vec)
  {
    std::string set_info_path = path_root + dataset_name + "/dataset.txt";
    std::ifstream info_file{set_info_path};
    std::string line_data;
    std::getline(info_file, line_data); // Line 1 is comment
    std::getline(info_file, line_data); // Line 2 is number of images in set
    std::istringstream iss{line_data};
    int image_number;
    iss >> image_number;
    std::cout << dataset_name << ": " << image_number << std::endl;

    std::vector<std::string> images_path;

    // Next line is a comment and the next image_number lines after that are the path to the images.
    // The line after the image path is another comment.
    std::getline(info_file, line_data); // Skip line
    for (int i = 0; i < image_number; i++)
    {
      std::getline(info_file, line_data); // Read image path
      while (!std::isalnum(line_data[line_data.size() - 1]))
      {
        line_data = std::string{line_data.begin(), line_data.end() - 1};
      }
      images_path.push_back("res/" + line_data);
    }
    std::getline(info_file, line_data); // Skip line
    // The (image_number-1) final lines corresponds to the homography between image 1 and image 2..N
    // Each line contains 9 floating point numbers to represent the 3x3 matrix (row-major)
    std::vector<std::array<float, 9>> homographies;
    homographies.resize(image_number - 1);
    for (int i = 0; i < image_number - 1; i++)
    {
      std::getline(info_file, line_data);
      iss = std::istringstream{line_data};
      iss >> homographies[i][0] >> homographies[i][1] >> homographies[i][2] >> homographies[i][3] >> homographies[i][4] >> homographies[i][5] >>
          homographies[i][6] >> homographies[i][7] >> homographies[i][8];

      for (int j = 0; j < 9; j++)
      {
        std::cout << homographies[i][j] << " ";
      }
      std::cout << std::endl;
    }
    // Release file as we have every info we need
    info_file.close();

    cv::Mat img1 = cv::imread(images_path[0]);
    for (int n = 1; n < image_number; n++)
    {
      std::vector<CommonPoint> kp_img1, kp_img2;
      std::vector<CommonPoint> matches_img1, matches_img2;

      cv::Mat imgN = cv::imread(images_path[n]);
      detector->getMatches(img1, imgN, kp_img1, kp_img2, matches_img1, matches_img2);
      cv::Mat matches_img = getKeypointsMatchesImage(img1, imgN, matches_img1, matches_img2);

      float putative_match_ratio, precision, matching_score, recall;

      // Get metrics
      computeMetrics(kp_img1, kp_img2, matches_img1, matches_img2, homographies[n - 1], putative_match_ratio, precision, matching_score, recall);
      // Write them to output file
      std::string res_str = dataset_name + ";" + std::to_string(1) + ";" + std::to_string(n + 1) + ";" + std::to_string(putative_match_ratio) + ";" +
                            std::to_string(precision) + ";" + std::to_string(matching_score) + ";" + std::to_string(recall) + "\n";
      result_file.write(res_str.c_str(), res_str.size());

      // cv::imshow("Test", matches_img);
      // cv::waitKey(30);
    }
  }

  detector->terminate();
  result_file.close();

  return 0;
}