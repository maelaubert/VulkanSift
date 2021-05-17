#include "test_utils.h"

cv::Mat getOrientedKeypointsImage(uint8_t *in_img, std::vector<vksift_Feature> kps, int width, int height)
{
  // OpenCV-like SIFT features drawing
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

cv::Mat getKeypointsMatchesImage(uint8_t *in_img1, std::vector<vksift_Feature> kps1, int width1, int height1, uint8_t *in_img2,
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

  for (size_t i = 0; i < kps1.size(); i++)
  {
    cv::Scalar color(rand() % 255, rand() % 255, rand() % 255, rand() % 255);
    cv::Point im1_p(kps1.at(i).orig_x, kps1.at(i).orig_y);
    cv::Point im2_origin(max_width, 0);
    cv::Point im2_p(kps2.at(i).orig_x, kps2.at(i).orig_y);
    cv::line(concatenated_mat, im1_p, im2_origin + im2_p, color, 2, cv::LINE_AA);
  }

  return concatenated_mat;
}

constexpr float DOG_MAX_VAL = 0.15f;
cv::Mat getColormappedDoGImage(cv::Mat image)
{
  cv::Mat out_image;
  image.copyTo(out_image);
  cv::cvtColor(out_image, out_image, cv::COLOR_GRAY2BGR);
  // out_image.create(image.rows, image.cols, CV_8UC3);
  for (int y = 0; y < image.rows; y++)
  {
    for (int x = 0; x < image.cols; x++)
    {
      float val = image.at<float>(cv::Point(x, y));
      if (val >= 0)
      {
        val = fminf(fabs(val) / DOG_MAX_VAL, 1.f);
        out_image.at<cv::Point3f>(cv::Point(x, y)) = cv::Point3f(0.f, val, 0.f);
      }
      else
      {
        val = fminf(fabs(val) / DOG_MAX_VAL, 1.f);
        out_image.at<cv::Point3f>(cv::Point(x, y)) = cv::Point3f(0.f, 0.f, val);
      }
    }
  }
  return out_image;
}