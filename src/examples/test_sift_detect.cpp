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

  while (vksift_presentDebugFrame(vksift_instance))
  {
    std::vector<vksift_Feature> feat_vec;
    vksift_detectFeatures(vksift_instance, image1, width1, height1, 0u);
    uint32_t nb_sift = vksift_getFeaturesNumber(vksift_instance, 0u);
    std::cout << "Feature found: " << nb_sift << std::endl;
    feat_vec.resize(nb_sift);
    vksift_downloadFeatures(vksift_instance, feat_vec.data(), 0u);

    cv::Mat draw_frame;
    img1_grey.convertTo(draw_frame, CV_8UC3);
    cv::cvtColor(draw_frame, draw_frame, cv::COLOR_GRAY2BGR);
    // cv::copyTo(cv_frame_color, draw_frame, cv::Mat());
    /*for (int i = 0; i < feat_vec.size(); i++)
    {
      cv::circle(draw_frame, cv::Point(feat_vec[i].orig_x, feat_vec[i].orig_y), 3, cv::Scalar(0, 0, 255), 1);
    }*/
    draw_frame = getOrientedKeypointsImage(image1, feat_vec, width1, height1);

    cv::imshow("VulkanSIFT keypoints", draw_frame);
    cv::waitKey(1);
  }

  delete[] image1;

  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}