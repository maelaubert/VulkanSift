extern "C"
{
#include <vulkansift/vulkansift.h>
}
#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

cv::Mat getOrientedKeypointsImage(uint8_t *in_img, std::vector<vksift_Feature> kps, int width, int height)
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

  for (vksift_Feature kp : kps)
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

cv::Mat getKeypointsMatches(uint8_t *in_img1, std::vector<vksift_Feature> kps1, int width1, int height1, uint8_t *in_img2,
                            std::vector<vksift_Feature> kps2, int width2, int height2)
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

  // Create concatenated image (update images to have the same size)
  int max_width = std::max(width1, width2);
  int max_height = std::max(height1, height2);
  cv::copyMakeBorder(output_mat_rgb1, output_mat_rgb1, 0, max_height - height1, 0, max_width - width1, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
  cv::copyMakeBorder(output_mat_rgb2, output_mat_rgb2, 0, max_height - height2, 0, max_width - width2, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

  cv::Mat concatenated_mat;
  cv::hconcat(output_mat_rgb1, output_mat_rgb2, concatenated_mat);

  srand(time(NULL));

  for (std::size_t i = 0; i < kps1.size(); i++)
  {
    cv::Scalar color(rand() % 255, rand() % 255, rand() % 255, rand() % 255);

    // output_mat.at<cv::Vec3b>(cv::Point(kp.orig_x, kp.orig_y)) = cv::Vec3b(0,
    // 0, 254); cv::circle(output_mat_rgb, cv::Point(kp.orig_x, kp.orig_y), 3,
    // cv::Scalar(0.0, 0.0, 255.0), 1);
    cv::Point im1_p(kps1.at(i).orig_x, kps1.at(i).orig_y);
    cv::Point im2_origin(max_width, 0);
    cv::Point im2_p(kps2.at(i).orig_x, kps2.at(i).orig_y);
    cv::line(concatenated_mat, im1_p, im2_origin + im2_p, color, 2, cv::LINE_AA);
  }

  return concatenated_mat;
}

int main()
{

  vksift_setLogLevel(VKSIFT_LOG_INFO);

  if (!vksift_loadVulkan())
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  vksift_Config config = vksift_Config_Default;
  vksift_Instance vksift_instance = NULL;
  if (!vksift_createInstance(&vksift_instance, &config))
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  // Read images with OpenCV
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

  //////////////////////

  while (vksift_presentDebugFrame(vksift_instance))
  {

    std::vector<vksift_Feature> img1_kp;
    std::vector<vksift_Feature> img2_kp;
    vksift_detectFeatures(vksift_instance, image1, width1, height1, 0u);
    vksift_detectFeatures(vksift_instance, image2, width2, height2, 1u);
    img1_kp.resize(vksift_getFeaturesNumber(vksift_instance, 0u));
    img2_kp.resize(vksift_getFeaturesNumber(vksift_instance, 1u));
    vksift_downloadFeatures(vksift_instance, img1_kp.data(), 0u);
    vksift_downloadFeatures(vksift_instance, img2_kp.data(), 1u);

    std::vector<vksift_Feature> matches_1;
    std::vector<vksift_Feature> matches_2;
    std::vector<vksift_Match_2NN> matches_info12;
    std::vector<vksift_Match_2NN> matches_info21;
    vksift_matchFeatures(vksift_instance, 0u, 1u);
    matches_info12.resize(vksift_getMatchesNumber(vksift_instance));
    std::cout << "matches_info12 size: " << matches_info12.size() << std::endl;
    vksift_downloadMatches(vksift_instance, matches_info12.data());

    vksift_matchFeatures(vksift_instance, 1u, 0u);
    matches_info21.resize(vksift_getMatchesNumber(vksift_instance));
    std::cout << "matches_info21 size: " << matches_info21.size() << std::endl;
    vksift_downloadMatches(vksift_instance, matches_info21.data());

    for (int i = 0; i < matches_info12.size(); i++)
    {
      int idx_in_2 = matches_info12[i].idx_b1;
      // Check mutual match
      if (matches_info21[idx_in_2].idx_b1 == i)
      {
        // Check Lowe's ratio in 1
        if ((matches_info12[i].dist_a_b1 / matches_info12[i].dist_a_b2) < 0.75)
        {
          // Check Lowe's ratio in 2
          if ((matches_info21[idx_in_2].dist_a_b1 / matches_info21[idx_in_2].dist_a_b2) < 0.75)
          {
            matches_1.push_back(img1_kp[matches_info12[i].idx_a]);
            matches_2.push_back(img2_kp[matches_info12[i].idx_b1]);
          }
        }
      }
    }
    std::cout << "Found " << matches_1.size() << " matches" << std::endl;

    cv::Mat draw_frame1, draw_frame2;
    img1_grey.convertTo(draw_frame1, CV_8UC3);
    img2_grey.convertTo(draw_frame2, CV_8UC3);
    cv::cvtColor(draw_frame1, draw_frame1, cv::COLOR_GRAY2BGR);
    cv::cvtColor(draw_frame2, draw_frame2, cv::COLOR_GRAY2BGR);
    draw_frame1 = getOrientedKeypointsImage(image1, img1_kp, width1, height1);
    draw_frame2 = getOrientedKeypointsImage(image2, img2_kp, width2, height2);

    cv::imshow("VulkanSIFT image1 keypoints", draw_frame1);
    cv::imshow("VulkanSIFT image2 keypoints", draw_frame2);

    // Draw matches
    cv::Mat matches_image = getKeypointsMatches(image1, matches_1, width1, height1, image2, matches_2, width2, height2);
    cv::imshow("VulkanSIFT matches", matches_image);

    cv::waitKey(1);
  }

  delete[] image1;
  delete[] image2;

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}