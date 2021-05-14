cmake_minimum_required(VERSION 3.0)
project(VulkanSift DESCRIPTION "SIFT C++ library" LANGUAGES CXX C)


#####################################################################################"
# Set default value for build type to Release
if(NOT CMAKE_BUILD_TYPE)
    message("CMAKE_BUILD_TYPE not specified. Build type will be set to Release.")
    set(CMAKE_BUILD_TYPE Release)
endif()

#####################################################################################

#####################################################################################"
# Define project options and variables
option(VULKANSIFT_BUILD_EXAMPLES "Use to build VulkanSift examples." OFF)
option(VULKANSIFT_BUILD_PERFS "Use to build VulkanSift performance tests." OFF)
set(VULKANSIFT_SANITIZE "" CACHE STRING "Use with value \"Address\" to build using the address sanitizer or 
    with \"Thread\" to build using the thread sanitizer. Leave empty otherwise.")


#####################################################################################"
# Check if compiler is Clang or GNU (avoids lots of trouble with optimization related parameters)
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Only GNU and Clang C++ compilers are supported in this project.")
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
    # TODO: only needed for GPU debugging, to add only if needed
    src/vulkansift/vkenv/vulkan_swapchain.c
    src/vulkansift/vkenv/debug_presenter.c
    src/vulkansift/vkenv/mini_window_xcb.c # only if using Linux
)
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
    # SIFT matching algirhtm shaders
    src/vulkansift/shaders/Get2NearestNeighbors.comp
)

#####################################################################################
#####################################################################################
# Handle CXX compiler and linker options
set(VULKANSIFT_COMPILER_OPTIONS "-Wall;-Wextra;-O3;$<$<CONFIG:Debug>:-g>;")
set(VULKANSIFT_LINKER_OPTIONS "")

#####################################################################################
# Add options for address and thread sanitizers (debug only) for GCC/CLANG compilers
# Use -DSANITIZE:BOOL=Address or -DSANITIZE:BOOL=Thread to activate with -DCMAKE_BUILD_TYPE:String=Debug
if(VULKANSIFT_SANITIZE STREQUAL Address)
    string(APPEND VULKANSIFT_COMPILER_OPTIONS "-fno-omit-frame-pointer;-fsanitize=address")
    string(APPEND VULKANSIFT_LINKER_OPTIONS "-fsanitize=address")
elseif(VULKANSIFT_SANITIZE STREQUAL Thread)
    string(APPEND VULKANSIFT_COMPILER_OPTIONS "-fno-omit-frame-pointer;-fsanitize=thread;")
    string(APPEND VULKANSIFT_LINKER_OPTIONS "-fsanitize=thread")
endif()
#####################################################################################
#####################################################################################
# Build the VulkanSift library (no dependencies except git submodules)
add_library(vulkansift STATIC ${VULKANSIFT_LIB_SRC})
target_compile_options(vulkansift PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET vulkansift APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")
set_target_properties(vulkansift PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
set_target_properties(vulkansift PROPERTIES PUBLIC_HEADER "${VULKANSIFT_LIB_HEADERS}")

target_include_directories(vulkansift PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/>)
target_compile_definitions(vulkansift PRIVATE VKSIFT_GPU_DEBUG=1 VK_USE_PLATFORM_XCB_KHR=1)# VK_NO_PROTOTYPES=1)
target_link_libraries(vulkansift
    dl xcb vulkan
)

# Compile shaders if modified and store them in build/shaders/
set(VULKANSIFT_LIB_SHADERS_SPV "")
foreach(shader_path ${VULKANSIFT_LIB_SHADERS})

    get_filename_component(shader_filename ${shader_path} NAME)
    message(${shader_filename})

    set(output_file ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_filename}.spv)
    message(${output_file})
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/shaders
        COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/${shader_path} -g -o ${output_file}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader_path}
        COMMENT "Building ${shader_filename}.spv --> ${CMAKE_CURRENT_SOURCE_DIR}/${shader_path} -g -o ${output_file}"
    )
    message(STATUS "Generating build command for ${shader_filename}.spv")
    set(VULKANSIFT_LIB_SHADERS_SPV ${VULKANSIFT_LIB_SHADERS_SPV} ${output_file} )
endforeach()
add_custom_target(vulkansift_shaders ALL DEPENDS ${VULKANSIFT_LIB_SHADERS_SPV})
add_dependencies(vulkansift vulkansift_shaders)

#####################################################################################

add_executable(test src/examples/test.cpp)
find_package(OpenCV REQUIRED)

target_compile_options(test PRIVATE "-O3")
target_link_libraries(test vulkansift ${OpenCV_LIBS})
target_compile_definitions(test PRIVATE VKSIFT_GPU_DEBUG=1)
target_compile_options(test PRIVATE "${VULKANSIFT_COMPILER_OPTIONS}")
set_property(TARGET test APPEND_STRING PROPERTY LINK_FLAGS " ${VULKANSIFT_LINKER_OPTIONS}")

if(VULKANSIFT_BUILD_EXAMPLES)
message("Examples will be built")

find_package(OpenCV REQUIRED)
# Add test executables
add_executable(test_sift_detect src/examples/test_sift_detect.cpp)
target_compile_options(test_sift_detect PRIVATE "-O3")
target_link_libraries(test_sift_detect vulkansift ${OpenCV_LIBS})

add_executable(test_sift_match src/examples/test_sift_match.cpp)
target_compile_options(test_sift_match PRIVATE "-O3")
target_link_libraries(test_sift_match vulkansift ${OpenCV_LIBS})



endif()

if(VULKANSIFT_BUILD_PERFS)
message("Performance tests will be built")
# Add perf executables

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
    ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/popsift/lib/libpopsift.so.1.0.0
)

add_executable(perf_sift_match src/perf/perf_matching.cpp ${VULKAN_SIFT_PERFS_COMMON_SRC})
target_compile_options(perf_sift_match PRIVATE "-O3;-fopenmp")
target_include_directories(perf_sift_match PRIVATE ${VULKAN_SIFT_PERFS_COMMON_INC})
target_link_libraries(perf_sift_match ${VULKAN_SIFT_PERFS_COMMON_LIBS})

add_executable(perf_sift_runtime src/perf/perf_runtime.cpp ${VULKAN_SIFT_PERFS_COMMON_SRC})
target_compile_options(perf_sift_runtime PRIVATE "-O3;-fopenmp")
target_include_directories(perf_sift_runtime PRIVATE ${VULKAN_SIFT_PERFS_COMMON_INC})
target_link_libraries(perf_sift_runtime ${VULKAN_SIFT_PERFS_COMMON_LIBS})

endif()

# Copy image resource directory to build folder if examples or perf scripts are built
if(VULKANSIFT_BUILD_EXAMPLES)
add_custom_command(TARGET test_sift_detect POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:test_sift_detect>/res)
elseif(VULKANSIFT_BUILD_PERFS)
add_custom_command(TARGET perf_sift_match POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:perf_sift_match>/res)
endif()