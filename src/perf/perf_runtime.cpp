#include "perf/perf_common.h"
#include <fstream>
#include <iostream>

#define NB_ITER_WARMUP 50
#define NB_ITER_MEAS 500

void printUsage()
{
  std::cout << "Usage: ./perf_sift_runtime SIFT_DETECTOR_NAME" << std::endl;
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
  std::ofstream result_file{"runtime_results_" + detector_name + ".txt"};

  cv::Mat image = cv::imread("res/img1.ppm");
  cv::resize(image, image, cv::Size(640, 480));

  float mean_exec_time = detector->measureMeanExecutionTimeMs(image, NB_ITER_WARMUP, NB_ITER_MEAS);
  // Write them to output file
  std::string res_str = std::to_string(mean_exec_time) + " ms" + "\n";
  result_file.write(res_str.c_str(), res_str.size());

  detector->terminate();
  result_file.close();

  return 0;
}