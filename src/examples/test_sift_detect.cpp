#include "vulkansift/viz/vulkan_viewer.h"
#include "vulkansift/vulkansift.h"

#include <iostream>
#include <opencv2/opencv.hpp>

cv::Mat getOrientedKeypointsImage(uint8_t *in_img, std::vector<VulkanSIFT::SIFT_Feature> kps, int width, int height)
{

  cv::Mat output_mat(height, width, CV_8U);

  for (int i = 0; i < width * height; i++)
  {
    output_mat.data[i] = in_img[i];
  }

  cv::Mat output_mat_rgb(height, width, CV_8UC3);
  output_mat.convertTo(output_mat_rgb, CV_8UC3);

  cv::cvtColor(output_mat_rgb, output_mat_rgb, cv::COLOR_GRAY2BGR);

  srand(time(NULL));

  for (VulkanSIFT::SIFT_Feature kp : kps)
  {
    cv::Scalar color(rand() % 255, rand() % 255, rand() % 255, rand() % 255);

    int radius = kp.sigma;
    cv::circle(output_mat_rgb, cv::Point(kp.orig_x, kp.orig_y), radius, color, 1);
    float angle = kp.theta;
    if (angle > 3.14f)
    {
      angle -= 2.f * 3.14f;
    }

    cv::Point orient(cosf(angle) * radius, sinf(angle) * radius);
    cv::line(output_mat_rgb, cv::Point(kp.orig_x, kp.orig_y), cv::Point(kp.orig_x, kp.orig_y) + orient, color, 1);
  }

  return output_mat_rgb;
}

cv::Mat getKeypointsMatches(uint8_t *in_img1, std::vector<VulkanSIFT::SIFT_Feature> kps1, int width1, int height1, uint8_t *in_img2,
                            std::vector<VulkanSIFT::SIFT_Feature> kps2, int width2, int height2)
{
  // Convert image 1
  cv::Mat output_mat1(height1, width1, CV_8U);
  for (int i = 0; i < width1 * height1; i++)
  {
    output_mat1.data[i] = in_img1[i];
  }
  cv::Mat output_mat_rgb1(height1, width1, CV_8UC3);
  output_mat1.convertTo(output_mat_rgb1, CV_8UC3);
  cv::cvtColor(output_mat_rgb1, output_mat_rgb1, cv::COLOR_GRAY2BGR);

  // Convert image 2
  cv::Mat output_mat2(height2, width2, CV_8U);
  for (int i = 0; i < width2 * height2; i++)
  {
    output_mat2.data[i] = in_img2[i];
  }
  cv::Mat output_mat_rgb2(height2, width2, CV_8UC3);
  output_mat2.convertTo(output_mat_rgb2, CV_8UC3);
  cv::cvtColor(output_mat_rgb2, output_mat_rgb2, cv::COLOR_GRAY2BGR);

  // Create concatenated image
  cv::Mat concatenated_mat;
  cv::hconcat(output_mat_rgb1, output_mat_rgb2, concatenated_mat);

  srand(time(NULL));

  for (int i = 0; i < kps1.size(); i++)
  {
    cv::Scalar color(rand() % 255, rand() % 255, rand() % 255, rand() % 255);

    // output_mat.at<cv::Vec3b>(cv::Point(kp.orig_x, kp.orig_y)) = cv::Vec3b(0, 0, 254);
    // cv::circle(output_mat_rgb, cv::Point(kp.orig_x, kp.orig_y), 3, cv::Scalar(0.0, 0.0, 255.0), 1);
    cv::Point im1_p(kps1.at(i).orig_x, kps1.at(i).orig_y);
    cv::Point im2_origin(width1, 0);
    cv::Point im2_p(kps2.at(i).orig_x, kps2.at(i).orig_y);
    cv::line(concatenated_mat, im1_p, im2_origin + im2_p, color, 1);
  }

  return concatenated_mat;
}
#define NB_HIST 4
#define NB_ORI 8
int main()
{

  VulkanInstance instance;
  if (!instance.init(640, 480, true, true))
  {
    std::cout << "Impossible to initialize VulkanInstance" << std::endl;
    return -1;
  }

  VulkanViewer viewer;
  if (!viewer.init(&instance, 640, 480))
  {
    std::cout << "Impossible to initialize VulkanViewer" << std::endl;
    return -1;
  }
  VulkanSIFT::SiftDetector detector;
  if (!detector.init(&instance, 640, 480))
  {
    std::cout << "Impossible to initialize SiftDetector" << std::endl;
    return -1;
  }

  ///////////////////////////
  // TEST MATCH
  bool do_match = false;
  if (do_match)
  {
    cv::Mat img1 = cv::imread("res/img1.ppm");
    cv::resize(img1, img1, cv::Size(640, 480));

    int width1 = img1.cols;
    int height1 = img1.rows;
    uint8_t *image1 = new uint8_t[img1.rows * img1.cols];
    cv::Mat img1_grey(cv::Size(width1, height1), CV_8UC1);
    cv::cvtColor(img1, img1, cv::COLOR_BGR2GRAY);
    img1.convertTo(img1_grey, CV_8UC1);
    for (int i = 0; i < img1_grey.cols; i++)
    {
      for (int j = 0; j < img1_grey.rows; j++)
      {
        image1[j * img1_grey.cols + i] = img1_grey.at<uint8_t>(cv::Point(i, j));
      }
    }

    cv::Mat img2 = cv::imread("res/img2.ppm");
    cv::resize(img2, img2, cv::Size(640, 480));

    int width2 = img2.cols;
    int height2 = img2.rows;
    uint8_t *image2 = new uint8_t[img2.rows * img2.cols];
    cv::Mat img2_grey(cv::Size(width2, height2), CV_8UC1);
    cv::cvtColor(img2, img2, cv::COLOR_BGR2GRAY);
    img2.convertTo(img2_grey, CV_8UC1);
    for (int i = 0; i < img2_grey.cols; i++)
    {
      for (int j = 0; j < img2_grey.rows; j++)
      {
        image2[j * img2_grey.cols + i] = img2_grey.at<uint8_t>(cv::Point(i, j));
      }
    }

    std::vector<VulkanSIFT::SIFT_Feature> img1_kp;
    std::vector<VulkanSIFT::SIFT_Feature> img2_kp;
    detector.compute(image1, img1_kp);
    detector.compute(image2, img2_kp);

    std::cout << img1_kp.size() << std::endl;
    std::cout << img2_kp.size() << std::endl;

    // Match keypoints between img1 and img2
    std::vector<VulkanSIFT::SIFT_Feature> match_img1;
    std::vector<VulkanSIFT::SIFT_Feature> match_img2;
    // float matching_threshold = 0.6f;
    float matching_threshold = 0.75f;

    // Compute keypoints distances first
    float *dist_arr = new float[img1_kp.size() * img2_kp.size()];
    for (int kp_idx1 = 0; kp_idx1 < img1_kp.size(); kp_idx1++)
    {
      for (int kp_idx2 = 0; kp_idx2 < img2_kp.size(); kp_idx2++)
      {
        float d = 0.f;
        for (int i = 0; i < NB_HIST * NB_HIST * NB_ORI; i++)
        {
          d += (img1_kp.at(kp_idx1).feature_vector[i] - img2_kp.at(kp_idx2).feature_vector[i]) *
               (img1_kp.at(kp_idx1).feature_vector[i] - img2_kp.at(kp_idx2).feature_vector[i]);
        }
        dist_arr[kp_idx1 * img2_kp.size() + kp_idx2] = sqrtf(d);
      }
    }
    // Search for nearest keypoints from first image keypoints
    for (int kp_idx1 = 0; kp_idx1 < img1_kp.size(); kp_idx1++)
    {

      float first_best_distance;
      int first_best_idx;
      float second_best_distance;
      int second_best_idx;

      if (dist_arr[kp_idx1 * img2_kp.size() + 0] < dist_arr[kp_idx1 * img2_kp.size() + 1])
      {
        first_best_distance = dist_arr[kp_idx1 * img2_kp.size() + 0];
        first_best_idx = 0;
        second_best_distance = dist_arr[kp_idx1 * img2_kp.size() + 1];
        second_best_idx = 1;
      }
      else
      {
        first_best_distance = dist_arr[kp_idx1 * img2_kp.size() + 1];
        first_best_idx = 1;
        second_best_distance = dist_arr[kp_idx1 * img2_kp.size() + 0];
        second_best_idx = 0;
      }

      // Find 2 nearest neighbors
      for (int kp_idx2 = 2; kp_idx2 < img2_kp.size(); kp_idx2++)
      {

        if (dist_arr[kp_idx1 * img2_kp.size() + kp_idx2] < first_best_distance)
        {
          second_best_distance = first_best_distance;
          second_best_idx = first_best_idx;
          first_best_distance = dist_arr[kp_idx1 * img2_kp.size() + kp_idx2];
          first_best_idx = kp_idx2;
        }
        else if (dist_arr[kp_idx1 * img2_kp.size() + kp_idx2] < second_best_distance)
        {
          second_best_distance = dist_arr[kp_idx1 * img2_kp.size() + kp_idx2];
          second_best_idx = kp_idx2;
        }
      }

      // Add matches
      if ((first_best_distance / second_best_distance) < matching_threshold)
      {
        match_img1.push_back(img1_kp.at(kp_idx1));
        match_img2.push_back(img2_kp.at(first_best_idx));
      }
    }
    delete[] dist_arr;

    // Draw matches
    cv::Mat matches_image = getKeypointsMatches(image1, match_img1, width1, height1, image2, match_img2, width2, height2);
    cv::imshow("Matches", matches_image);
    cv::waitKey(0);
    delete[] image1;
    delete[] image2;
  }
  //////////////////////

  // Read image with OpenCV
  cv::Mat img1 = cv::imread("res/img1.ppm");
  cv::resize(img1, img1, cv::Size(640, 480));

  int width1 = img1.cols;
  int height1 = img1.rows;
  uint8_t *image1 = new uint8_t[img1.rows * img1.cols];
  cv::Mat img1_grey(cv::Size(width1, height1), CV_8UC1);
  cv::cvtColor(img1, img1, cv::COLOR_BGR2GRAY);
  img1.convertTo(img1_grey, CV_8UC1);
  for (int i = 0; i < img1_grey.cols; i++)
  {
    for (int j = 0; j < img1_grey.rows; j++)
    {
      image1[j * img1_grey.cols + i] = img1_grey.at<uint8_t>(cv::Point(i, j));
    }
  }

  while (!viewer.shouldStop())
  {
    std::vector<VulkanSIFT::SIFT_Feature> feat_vec;
    detector.compute(image1, feat_vec);

    cv::Mat draw_frame;
    img1_grey.convertTo(draw_frame, CV_8UC3);
    cv::cvtColor(draw_frame, draw_frame, cv::COLOR_GRAY2BGR);
    // cv::copyTo(cv_frame_color, draw_frame, cv::Mat());
    /*for (int i = 0; i < feat_vec.size(); i++)
    {
      cv::circle(draw_frame, cv::Point(feat_vec[i].orig_x, feat_vec[i].orig_y), 3, cv::Scalar(0, 0, 255), 1);
    }*/
    draw_frame = getOrientedKeypointsImage(image1, feat_vec, width1, height1);

    cv::imshow("Test", draw_frame);
    cv::waitKey(30);

    float gpu_time;
    viewer.execOnce(image1, &gpu_time);
  }

  delete[] image1;

  detector.terminate();
  viewer.terminate();
  instance.terminate();

  return 0;
}