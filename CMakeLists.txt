cmake_minimum_required(VERSION 3.0)
project(VulkanSift DESCRIPTION "SIFT C++ library" LANGUAGES CXX C)


#####################################################################################"
# Set default value for build type to Release
if(NOT CMAKE_BUILD_TYPE)
    message("CMAKE_BUILD_TYPE not specified. Build type will be set to Release.")
    set(CMAKE_BUILD_TYPE Release)
endif()

#####################################################################################
# Find the Vulkan SDK
if(NOT CMAKE_VERSION VERSION_LESS 3.7.0) #if FindCmake is available
    find_package(Vulkan REQUIRED)
elseif(DEFINED ENV{VULKAN_SDK}) # if VULKAN_SDK in the path
    set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/lib")
    find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS "$ENV{VULKAN_SDK}/lib")
else()
    message(FATAL_ERROR "Could not find the Vulkan SDK")
endif()

#####################################################################################"
# Define project options and variables
option(VULKANSIFT_WITH_GPU_DEBUG "Use to compile VulkanSift for GPU debug" OFF)
option(VULKANSIFT_WITH_EMBEDDED_SHADERS "Use to embed the SPIRV shader code directly inside the binary" ON)
option(VULKANSIFT_WITH_VK_LOAD_AT_RUNTIME "Use to dynamically load the Vulkan library at runtime" ON)
option(VULKANSIFT_BUILD_EXAMPLES "Use to build VulkanSift examples." OFF)
option(VULKANSIFT_BUILD_PERFS "Use to build VulkanSift performance tests." OFF)
set(VULKANSIFT_SANITIZE "" CACHE STRING "Use with value \"Address\" to build using the address sanitizer or 
    with \"Thread\" to build using the thread sanitizer. Leave empty otherwise.")

#####################################################################################"
# Check if compiler is Clang or GNU (avoids lots of trouble with optimization related parameters)
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|MSVC")
     message(FATAL_ERROR "Only GNU, Clang and MSVC compilers are supported in this project.")
endif()

# Check if platform is Linux or Windows (would need MoltenVK for Mac)
if (NOT CMAKE_SYSTEM_NAME MATCHES "Linux|Windows")
    message(FATAL_ERROR "Only Linux and Windows platforms are supported in this project.")
endif()
#####################################################################################
# VulkanSift library sources
set(VULKANSIFT_LIB_SRC
    src/vulkansift/vulkansift.c
    src/vulkansift/sift_memory.c
    src/vulkansift/sift_detector.c
    src/vulkansift/sift_matcher.c
    # relevant vkenv files
    src/vulkansift/vkenv/vulkan_loader.c
    src/vulkansift/vkenv/vulkan_device.c
    src/vulkansift/vkenv/vulkan_utils.c
    src/vulkansift/vkenv/logger.c
)

if(VULKANSIFT_WITH_GPU_DEBUG)
    set(VULKANSIFT_LIB_SRC ${VULKANSIFT_LIB_SRC}
        # Add files used onle to provide GPU debugging/profiling capabilities
        src/vulkansift/vkenv/vulkan_swapchain.c
        src/vulkansift/vkenv/debug_presenter.c
    )
    # Add platform specific sources
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(VULKANSIFT_LIB_SRC ${VULKANSIFT_LIB_SRC} src/vulkansift/vkenv/mini_window_xcb.c)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
        # TODO
    endif()

endif()

if(VULKANSIFT_WITH_EMBEDDED_SHADERS)
    # if we use embedded shaders we need to add the source storing the spv to the library
    set(VULKANSIFT_LIB_SRC ${VULKANSIFT_LIB_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkansift/vkenv/embedded_shader_provider.c)
endif()

set(VULKANSIFT_LIB_HEADERS
    src/vulkansift/vulkansift.h
)

set(VULKANSIFT_LIB_SHADERS
    # SIFT detector algorithm shaders
    src/vulkansift/shaders/GaussianBlur.comp
    src/vulkansift/shaders/GaussianBlurInterpolated.comp
    src/vulkansift/shaders/DifferenceOfGaussian.comp
    src/vulkansift/shaders/ExtractKeypoints.comp
    src/vulkansift/shaders/ComputeOrientation.comp
    src/vulkansift/shaders/ComputeDescriptors.comp
    # SIFT matching algorithm shaders
    src/vulkansift/shaders/Get2NearestNeighbors.comp
)

#####################################################################################
#####################################################################################
# Handle CXX compiler and linker options
set(VULKANSIFT_COMPILER_OPTIONS "-Wall;-O3;$<$<CONFIG:Debug>:-g>;")
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    string(APPEND VULKANSIFT_COMPILER_OPTIONS "-Wextra;")
endif()
set(VULKANSIFT_LINKER_OPTIONS "")

#####################################################################################
# Add options for address and thread sanitizers (debug only) for GCC/CLANG compilers
# Use -DSANITIZE:BOOL=Address or -DSANITIZE:BOOL=Thread to activate with -DCMAKE_BUILD_TYPE:String=Debug
if(VULKANSIFT_SANITIZE STREQUAL Address)
    string(APPEND VULKANSIFT_COMPILER_OPTIONS "-fsanitize=address")
    string(APPEND VULKANSIFT_LINKER_OPTIONS "-fsanitize=address")
elseif(VULKANSIFT_SANITIZE STREQUAL Thread)
    string(APPEND VULKANSIFT_COMPILER_OPTIONS "-fsanitize=thread;")
    string(APPEND VULKANSIFT_LINKER_OPTIONS "-fsanitize=thread")
endif()
#####################################################################################
#####################################################################################
# Build the VulkanSift library (no dependencies except git submodules)
add_library(vulkansift STATIC ${VULKANSIFT_LIB_SRC})
target_compile_options(vulkansift PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET vulkansift APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")
set_target_properties(vulkansift PROPERTIES PUBLIC_HEADER "${VULKANSIFT_LIB_HEADERS}")
target_include_directories(vulkansift PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/>)
target_include_directories(vulkansift PRIVATE ${Vulkan_INCLUDE_DIRS})

if(VULKANSIFT_WITH_VK_LOAD_AT_RUNTIME)
    target_compile_definitions(vulkansift PRIVATE VK_NO_PROTOTYPES=1)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
        target_link_libraries(vulkansift dl)
    endif()
else()
    target_link_libraries(vulkansift  ${Vulkan_LIBRARY})
endif()

if(VULKANSIFT_WITH_GPU_DEBUG)
    target_compile_definitions(vulkansift PRIVATE VKSIFT_GPU_DEBUG=1)
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        # Only xcb is supported in Linux for now
        target_link_libraries(vulkansift xcb)
        target_compile_definitions(vulkansift PRIVATE VK_USE_PLATFORM_XCB_KHR=1)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
        # TODO
    endif()
endif()

# Add dependencies on shader files
include("cmake/ShaderUtils.cmake")
if(VULKANSIFT_WITH_EMBEDDED_SHADERS)
    addEmbeddedSpvShaderDependency(vulkansift "${VULKANSIFT_LIB_SHADERS}" ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkansift/vkenv/embedded_shader_provider.c)
    target_compile_definitions(vulkansift PRIVATE VKENV_USE_EMBEDDED_SHADERS=1)
else()
    addSpvShaderFilesDependency(vulkansift "${VULKANSIFT_LIB_SHADERS}")
endif()
#####################################################################################



if(VULKANSIFT_BUILD_EXAMPLES)
message(STATUS "Examples will be built")
find_package(OpenCV REQUIRED)
# Example program that read an image, detect and draw the SIFT features
add_executable(test_sift_detect src/examples/test_sift_detect.cpp src/examples/test_utils.cpp)
target_compile_options(test_sift_detect PRIVATE "-O3")
target_link_libraries(test_sift_detect vulkansift ${OpenCV_LIBS})
target_compile_options(test_sift_detect PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET test_sift_detect APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")

# Example program that read two images and perform SIFT feature matching
add_executable(test_sift_match src/examples/test_sift_match.cpp src/examples/test_utils.cpp)
target_compile_options(test_sift_match PRIVATE "-O3")
target_link_libraries(test_sift_match vulkansift ${OpenCV_LIBS})
target_compile_options(test_sift_match PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET test_sift_match APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")

# Example program that detect SIFT features and show the scale-space pyramid images
add_executable(test_sift_show_pyr src/examples/test_sift_show_pyr.cpp src/examples/test_utils.cpp)
target_compile_options(test_sift_show_pyr PRIVATE "-O3")
target_link_libraries(test_sift_show_pyr vulkansift ${OpenCV_LIBS})
target_compile_options(test_sift_show_pyr PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET test_sift_show_pyr APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")

if(VULKANSIFT_WITH_GPU_DEBUG)
    # Example program that enables GPU debug/profiling with external tools (NVIDIA Nsight, Renderdoc)
    add_executable(test_sift_gpu_debug src/examples/test_sift_gpu_debug.cpp src/examples/test_utils.cpp)
    target_compile_options(test_sift_gpu_debug PRIVATE "-O3")
    target_link_libraries(test_sift_gpu_debug vulkansift ${OpenCV_LIBS})
    target_compile_options(test_sift_gpu_debug PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
    set_property(TARGET test_sift_gpu_debug APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")
endif(VULKANSIFT_WITH_GPU_DEBUG)
endif(VULKANSIFT_BUILD_EXAMPLES)

if(VULKANSIFT_BUILD_PERFS)
message(STATUS "Performance tests will be built")
# Add perf executables
find_package(OpenCV REQUIRED)
set(VULKAN_SIFT_PERFS_COMMON_SRC 
    src/perf/perf_common.cpp
    src/perf/wrappers/vulkansift_wrapper.cpp
    src/perf/wrappers/opencv_wrapper.cpp
    src/perf/wrappers/vlfeat_wrapper.cpp
    src/perf/wrappers/siftgpu_wrapper.cpp
    src/perf/wrappers/popsift_wrapper.cpp
)
set(VULKAN_SIFT_PERFS_COMMON_INC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/external_libs/>
)
set(VULKAN_SIFT_PERFS_COMMON_LIBS
    vulkansift
    pthread
    omp
    GL
    GLEW
    cuda
    cudart
    X11
    ${OpenCV_LIBS}
    ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/vlfeat/lib/libvlfeat.a
    ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/SiftGPU/lib/libsift_gpu.a
    ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/popsift/lib/libpopsift.so
)
add_executable(perf_sift_match src/perf/perf_matching.cpp ${VULKAN_SIFT_PERFS_COMMON_SRC})
target_compile_options(perf_sift_match PRIVATE "-O3;-fopenmp")
target_include_directories(perf_sift_match PRIVATE ${VULKAN_SIFT_PERFS_COMMON_INC})
target_link_libraries(perf_sift_match ${VULKAN_SIFT_PERFS_COMMON_LIBS})
target_compile_options(perf_sift_match PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET perf_sift_match APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")

add_executable(perf_sift_runtime src/perf/perf_runtime.cpp ${VULKAN_SIFT_PERFS_COMMON_SRC})
target_compile_options(perf_sift_runtime PRIVATE "-O3;-fopenmp")
target_include_directories(perf_sift_runtime PRIVATE ${VULKAN_SIFT_PERFS_COMMON_INC})
target_link_libraries(perf_sift_runtime ${VULKAN_SIFT_PERFS_COMMON_LIBS})
target_compile_options(perf_sift_runtime PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET perf_sift_runtime APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")

endif()

# Copy image resource directory to build folder if examples or perf scripts are built
if(VULKANSIFT_BUILD_EXAMPLES OR VULKANSIFT_BUILD_PERFS)

# Add generation information for resource files in build folder
file(GLOB_RECURSE RES_FILE_REL_PATHS RELATIVE ${CMAKE_SOURCE_DIR} res/*)
message(STATUS "Generating copy commands for resource files")
set(VULKANSIFT_RES_FILES "")
foreach(res_file_rel_path ${RES_FILE_REL_PATHS})
    set(output_file ${CMAKE_CURRENT_BINARY_DIR}/${res_file_rel_path})
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/${res_file_rel_path} ${CMAKE_CURRENT_BINARY_DIR}/${res_file_rel_path}
        DEPENDS ${CMAKE_SOURCE_DIR}/${res_file_rel_path}
        COMMENT "Copying resource ${res_file_rel_path} to build folder"
    )
    set(VULKANSIFT_RES_FILES ${VULKANSIFT_RES_FILES} ${output_file} )
endforeach()

if(VULKANSIFT_BUILD_EXAMPLES)
    # custom empty target that depends on all the resource files in the build folder
    add_custom_target(vulkansift_res_copy ALL DEPENDS ${VULKANSIFT_RES_FILES}) 
    # Add resource dependency to all test executables
    add_dependencies(test_sift_detect vulkansift_res_copy)
    add_dependencies(test_sift_match vulkansift_res_copy)
    add_dependencies(test_sift_show_pyr vulkansift_res_copy)
    if(VULKANSIFT_WITH_GPU_DEBUG)
        add_dependencies(test_sift_gpu_debug vulkansift_res_copy)
    endif()
endif()

if(VULKANSIFT_BUILD_PERFS)
    # Add resource dependency to all perf executables
    add_dependencies(perf_sift_match vulkansift_res_copy)
    add_dependencies(perf_sift_runtime vulkansift_res_copy)
endif()

endif()