#ifndef VKSIFT_TEST_UTILS
#define VKSIFT_TEST_UTILS

#include <opencv2/opencv.hpp>
#include <vector>

#include "vulkansift/vulkansift.h"

cv::Mat getOrientedKeypointsImage(uint8_t *in_img, std::vector<vksift_Feature> kps, int width, int height);
cv::Mat getKeypointsMatchesImage(uint8_t *in_img1, std::vector<vksift_Feature> kps1, int width1, int height1, uint8_t *in_img2,
                                 std::vector<vksift_Feature> kps2, int width2, int height2);
cv::Mat getColormappedDoGImage(cv::Mat image);

#endif // VKSIFT_TEST_UTILS