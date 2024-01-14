# VulkanSift

VulkanSift is a GPU implementation of the Scale-Invariant Feature Transform algorithm by David G. Lowe. The library is written in C and uses Vulkan to run SIFT feature detection and matching algorithms on any GPU supporting the Vulkan API.

Main features:
- SIFT feature detection and matching (2 nearest-neighbors brute-force matching)
- Configurable SIFT implementation (upsampling option, scalespace dimension, kernels sigma, extremum detection thresholds)
- Multiple descriptors format supported for compatibility with descriptors from other implementations
- Non-blocking API for time-consuming functions (detect/match)

Detection speed and feature comparison with other implementations are detailed in the [Performances](docs/Performances.md) page.

----
Note (31/03/21): as of today, VulkanSIFT has been tested with the following GPU/platforms:
- NVIDIA RTX 4070 (laptop): Linux/Windows
- NVIDIA RTX 2060 (laptop): Linux/Windows
- NVIDIA GTX 960M (laptop): Linux
- Qualcomm Adreno 615 (smartphone): Android
- Apple M2 (laptop): MacOS

## Dependencies

By default, VulkanSift doesn't have any dependency and load the Vulkan library  at runtime. If the Vulkan requirements are not met at runtime, the library setup calls will gracefully fail to let users fall back to other SIFT implementations.

The library examples use [OpenCV](https://opencv.org/) to read image files and display results. 

(VulkanSift development only) [GLFW3](https://www.glfw.org/) is used in an optional test program to provide an interface to any supported OS window manager. This is required to allow VulkanSift calls to be debugged/profiled with GPU tools (NVIDIA Nsight, RenderDoc, etc).

Optionnal performance test programs need to link and include files from other SIFT implementations ([VLFeat](https://www.vlfeat.org/), [SiftGPU](https://github.com/pitzer/SiftGPU), [PopSift](https://github.com/alicevision/popsift), [OpenCV](https://opencv.org/)). Information on how to add the required files to this repository are present in [Performances](docs/Performances.md).

## Build

### Requirements

- [CMake](https://cmake.org/) (version >= 3.0)
- [Python3](https://www.python.org/)
- [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)

### Clone

Clone the github repository and its submodules:
```shell
git clone --recurse-submodules git@github.com:maelaubert/VulkanSift.git
```

### Build

To build in a build/ folder in the repository folder:
```shell
mkdir build && cd build
cmake ..
make 
```
This CMake project provides the following options that you can use with your `cmake` command:
| Option                            | Default | Description                                                                                                                                     |
| --------------------------------- | ------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| VULKANSIFT_LOAD_VK_AT_RUNTIME     | ON      | If set to ON the VulkanSift library loads the Vulkan library at runtime. Otherwise, the vulkan libary is linked dynamically during compilation. |
| VULKANSIFT_BUILD_EXAMPLES         | OFF     | Define if examples are built.                                                                                                                   |
| VULKANSIFT_WITH_GPU_DEBUG_EXAMPLE | OFF     | Define if the example debuggable with NSight/RenderDoc/etc is built.                                                                            |
| VULKANSIFT_BUILD_PERFS            | OFF     | Define if the performance test programs should be built.                                                                                        |
| VULKANSIFT_SANITIZE               | " "     | Compile and link with AddressSanitizer if set to "Address".                                                                                     |
| BUILD_SHARED_LIBS                 | ON      | Define if library is built as a shared or static library.                                                                                       |

### Install

To install the CMake project (binaries, library file and include files) in the folder defined by the `CMAKE_INSTALL_PREFIX` variable:
```shell
# depending on CMAKE_INSTALL_PREFIX admin rights may be needed
make install
```

## Usage
### With CMake projects

If VulkanSift is used from another CMake project and has been installed you can use the CMake command:
```cmake
find_package(VulkanSift)
target_link_libraries(TARGET ${VulkanSift_LIB})
target_include_directories(TARGET PRIVATE ${VulkanSift_INCLUDE_DIR})
```
If not found automatically (may happen with custom install location) the CMake option VulkanSift_DIR can be set to specify the path to the folder containing VulkanSiftConfig.cmake.

If VulkanSift is not installed, you can place VulkanSift direcly inside your CMake build tree and use:
```cmake
add_subdirectory(PATH_TO_VULKANSIFT/VulkanSift)
target_link_libraries(TARGET ${VulkanSift_LIB})
target_include_directories(TARGET PRIVATE ${VulkanSift_INCLUDE_DIR})
```

### Basic feature detection code example (C++)

```cpp
#include <vulkansift/vulkansift.h>

int main()
{
  // Load and get acces to the Vulkan API
  if (vksift_loadVulkan() != VKSIFT_SUCCESS)
  {
    // Vulkan library not found, there may be no device supporting Vulkan
    // on this machine or something failed when setting up the Vulkan API access.
    return -1;
  }

  // Create VulkanSift instance using the default configuration (automatically selected GPU)
  vksift_Config config = vksift_getDefaultConfig();
  vksift_Instance vksift_instance = nullptr;
  if (vksift_createInstance(&vksift_instance, &config) != VKSIFT_SUCCESS)
  {
    // May fail if selected GPU doesn't support the application requirement or if something
    // went wrong when setting up/preparing GPU ressource memory or detection/matching pipelines.
    return -1;
  }

  uint8_t *image_data;
  uint32_t image_width;
  uint32_t image_height;
  // Fill above variables with image data/information here...

  // Start feature detection on the image, store the detection results in the GPU buffer 0
  uint32_t gpu_buffer_idx = 0u;
  vksift_detectFeatures(vksift_instance, image_data, image_width, image_height, gpu_buffer_idx);
  // Setup and resize a std::vector to store exactly the number of detected features
  std::vector<vksift_Feature> sift_features;
  sift_features.resize(vksift_getFeaturesNumber(vksift_instance, gpu_buffer_idx));
  // Download the detection results into the std::vector buffer
  vksift_downloadFeatures(vksift_instance, sift_features.data(), gpu_buffer_idx);

  // Destroy VulkanSift instance and release Vulkan API
  vksift_destroyInstance(&vksift_instance);
  vksift_unloadVulkan();

  return 0;
}
```

### Examples

The following examples are available in the [src/examples](./src/examples) folder:
- [test_sift_detect](./src/examples/test_sift_detect.cpp): detect and display SIFT features.
- [test_sift_match](./src/examples/test_sift_match.cpp): detect features from two images on two different GPU buffer, perform brute-force feature descriptor matching between the two buffer, download and filter the results on the CPU (cross-check, Lowe's ratio check) and display the valid matches.
- [test_sift_show_pyr](./src/examples/test_sift_show_pyr.cpp): detect SIFT feature on the input image, then display interactive visualization of the pyramid images downloaded from the GPU (scale-space images, Difference of Gaussian images).
- [test_sift_error_handling](./src/examples/test_sift_error_handling.cpp): shows how to configure a vksift_Instance to use a user-provided error callback function that throws an exception on errors. This code intentionally causes an invalid input error and shows that exceptions are used.
