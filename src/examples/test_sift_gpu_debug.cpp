#include "test_utils.h"
#include "vulkansift/vulkansift.h"

#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    std::cout << "Invalid command." << std::endl;
    std::cout << "Usage: ./test_sift_gpu_debug PATH_TO_IMAGE" << std::endl;
    return -1;
  }

  // Read image with OpenCV
  cv::Mat image = cv::imread(argv[1], 0);
  if (image.empty())
  {
    std::cout << "Failed to read image " << argv[1] << ". Stopping program." << std::endl;
    return -1;
  }

  if (!glfwInit())
  {
    std::cout << "glfwInit() failed." << std::endl;
    return -1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *glfw_window = glfwCreateWindow(400, 100, "vksift GPU debug", NULL, NULL);
  if (!glfw_window)
  {
    std::cout << "glfw window creation failed." << std::endl;
    glfwTerminate();
    return -1;
  }

  // Retrieve information about the window created by GLFW (needed by vksift to render to the window)
  vksift_ExternalWindowInfo window_info;
#ifdef _WIN32
  HINSTANCE win32_instance = GetModuleHandle(NULL);
  HWND win32_window = glfwGetWin32Window(glfw_window);
  window_info.context = (void *)(&win32_instance);
  window_info.window = (void *)(&win32_window);
#else
  Display *x11_display = glfwGetX11Display();
  Window x11_window = glfwGetX11Window(glfw_window);
  window_info.context = (void *)(&x11_display);
  window_info.window = (void *)(&x11_window);
#endif

  vksift_setLogLevel(VKSIFT_LOG_DEBUG);

  if (vksift_loadVulkan() != VKSIFT_ERROR_TYPE_SUCCESS)
  {
    std::cout << "Impossible to initialize the Vulkan API" << std::endl;
    return -1;
  }

  vksift_Config config = vksift_getDefaultConfig();
  config.input_image_max_size = image.cols * image.rows;
  config.use_gpu_debug_functions = true;
  config.gpu_debug_external_window_info = window_info;

  vksift_Instance vksift_instance = NULL;
  if (vksift_createInstance(&vksift_instance, &config) != VKSIFT_ERROR_TYPE_SUCCESS)
  {
    std::cout << "Impossible to create the vksift_instance" << std::endl;
    vksift_unloadVulkan();
    return -1;
  }

  std::vector<vksift_Feature> feats;

  while (!glfwWindowShouldClose(glfw_window)) // Loop until the user destroys the window
  {
    // Calling vksift_presentDebugFrame() draw an empty to the empty window, every GPU commands executed between two frame
    // draw (what's inside the while loop) can be profiled/debug with GPU debugger (Nsight, Renderdoc, and probably other tools)
    vksift_presentDebugFrame(vksift_instance);
    auto start_ts = std::chrono::high_resolution_clock::now();
    vksift_detectFeatures(vksift_instance, image.data, image.cols, image.rows, 0u);
    auto detect1_ts = std::chrono::high_resolution_clock::now();
    vksift_detectFeatures(vksift_instance, image.data, image.cols, image.rows, 1u);
    auto detect2_ts = std::chrono::high_resolution_clock::now();

    uint32_t nb_sift = vksift_getFeaturesNumber(vksift_instance, 0u);
    std::cout << "Feature found: " << nb_sift << std::endl;
    feats.resize(nb_sift);
    vksift_downloadFeatures(vksift_instance, feats.data(), 0u);
    auto download1_ts = std::chrono::high_resolution_clock::now();

    vksift_uploadFeatures(vksift_instance, feats.data(), feats.size(), 0);
    auto upload1_ts = std::chrono::high_resolution_clock::now();

    cv::Mat frame = getOrientedKeypointsImage(image.data, feats, image.cols, image.rows);
    cv::imshow("test", frame);
    cv::waitKey(1);

    vksift_downloadFeatures(vksift_instance, feats.data(), 1u);
    auto download2_ts = std::chrono::high_resolution_clock::now();

    vksift_uploadFeatures(vksift_instance, feats.data(), feats.size(), 1);
    auto upload2_ts = std::chrono::high_resolution_clock::now();

    std::cout << "Time to detect1: " << std::chrono::duration_cast<std::chrono::microseconds>(detect1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to detect2: " << std::chrono::duration_cast<std::chrono::microseconds>(detect2_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to download1: " << std::chrono::duration_cast<std::chrono::microseconds>(download1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to download2: " << std::chrono::duration_cast<std::chrono::microseconds>(download2_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to upload1: " << std::chrono::duration_cast<std::chrono::microseconds>(upload1_ts - start_ts).count() / 1000.f << std::endl;
    std::cout << "Time to upload2: " << std::chrono::duration_cast<std::chrono::microseconds>(upload2_ts - start_ts).count() / 1000.f << std::endl;

    vksift_matchFeatures(vksift_instance, 0u, 1u);
    uint32_t match_number = vksift_getMatchesNumber(vksift_instance);
    std::cout << "Matches found: " << match_number << std::endl;
    // buffer_idx = (buffer_idx + 1) % config.sift_buffer_count;

    glfwPollEvents(); // check for event (window destruction event)
  }

  // clean up vksift
  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  // clean up glfw
  glfwDestroyWindow(glfw_window);
  glfwTerminate();

  return 0;
}