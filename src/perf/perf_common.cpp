#include "perf/perf_common.h"
#include "perf/wrappers/opencv_wrapper.h"
#include "perf/wrappers/popsift_wrapper.h"
#include "perf/wrappers/siftgpu_wrapper.h"
#include "perf/wrappers/vlfeat_wrapper.h"
#include "perf/wrappers/vulkansift_wrapper.h"

static const std::vector<std::string> detector_name_list = {"VulkanSIFT", "OpenCV", "VLFeat", "SiftGPU", "PopSift"};

std::vector<std::string> getDetectorTypeNames() { return detector_name_list; }

bool getDetectorTypeFromName(std::string det_name, DETECTOR_TYPE &det_type)
{
  if (det_name == std::string{"VulkanSIFT"})
  {
    det_type = DETECTOR_TYPE::VULKANSIFT;
    return true;
  }
  else if (det_name == std::string{"OpenCV"})
  {
    det_type = DETECTOR_TYPE::OPENCV;
    return true;
  }
  else if (det_name == std::string{"VLFeat"})
  {
    det_type = DETECTOR_TYPE::VLFEAT;
    return true;
  }
  else if (det_name == std::string{"SiftGPU"})
  {
    det_type = DETECTOR_TYPE::SIFTGPU;
    return true;
  }
  else if (det_name == std::string{"PopSift"})
  {
    det_type = DETECTOR_TYPE::POPSIFT;
    return true;
  }
  return false;
}

std::shared_ptr<AbstractSiftDetector> createDetector(DETECTOR_TYPE type)
{
  switch (type)
  {
  case DETECTOR_TYPE::VULKANSIFT:
    return std::make_shared<VulkanSiftDetector>();
    break;
  case DETECTOR_TYPE::OPENCV:
    return std::make_shared<OpenCvDetector>();
  case DETECTOR_TYPE::VLFEAT:
    return std::make_shared<VLFeatDetector>();
  case DETECTOR_TYPE::SIFTGPU:
    return std::make_shared<SiftGPUDetector>();
  case DETECTOR_TYPE::POPSIFT:
    return std::make_shared<PopSiftDetector>();
  default:
    break;
  }
}