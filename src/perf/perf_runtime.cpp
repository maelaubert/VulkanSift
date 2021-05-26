#include "perf/perf_common.h"
#include <fstream>
#include <iostream>

#define NB_ITER_WARMUP 50
#define NB_ITER_MEAS 500

void printUsage()
{
  std::cout << "Usage: ./perf_sift_runtime IMAGE_PATH SIFT_DETECTOR_NAME" << std::endl;
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
  if (argc != 3)
  {
    std::cout << "Error: wrong number of arguments" << std::endl;
    printUsage();
    return -1;
  }

  std::string image_path{argv[1]};

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
  std::shared_ptr<AbstractSiftDetector> detector = createDetector(detector_type);
  detector->init();

  // Prepare output file
  std::ofstream result_file{"runtime_results_" + detector_name + ".txt"};

  cv::Mat image = cv::imread(image_path, 0);
  if (image.empty())
  {
    std::cout << "Failed to read image " << image_path << std::endl;
    return -1;
  }

  if (detector->useFloatImage())
  {
    image.convertTo(image, CV_32FC1);
  }

  std::vector<cv::KeyPoint> keypoints;
  cv::Mat desc;

  for (int i = 0; i < NB_ITER_WARMUP; i++)
  {
    detector->detectSIFT(image, keypoints, desc, false);
    std::cout << "\rWarmup " << i + 1 << "/" << NB_ITER_WARMUP;
  }
  std::cout << std::endl;

  float sum_duration = 0;
  for (int i = 0; i < NB_ITER_MEAS; i++)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    detector->detectSIFT(image, keypoints, desc, false);
    float duration =
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count()) / 1000.f;
    sum_duration += duration;
    std::cout << "\rMeasuring " << i + 1 << "/" << NB_ITER_MEAS;
  }
  std::cout << std::endl;
  float mean_duration = float(sum_duration) / float(NB_ITER_MEAS);

  // Write them to output file
  std::string res_str = std::to_string(mean_duration) + " ms" + "\n";
  result_file.write(res_str.c_str(), res_str.size());

  detector->terminate();
  result_file.close();

  return 0;
}