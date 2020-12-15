cmake_minimum_required(VERSION 3.0)
project(VulkanSift DESCRIPTION "SIFT C++ library" LANGUAGES CXX)


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
    src/vulkansift/vulkansift.cpp
    src/vulkansift/vulkan_instance.cpp
    src/vulkansift/sift/sift_detector.cpp
    src/vulkansift/viz/vulkan_viewer.cpp
    src/vulkansift/utils/vulkan_utils.cpp
    src/vulkansift/utils/vulkan_loader.cpp
    src/vulkansift/utils/GLFW_loader.cpp
)
set(VULKANSIFT_LIB_HEADERS
    src/vulkansift/vulkansift.h
)

set(VULKANSIFT_LIB_SHADERS
    src/vulkansift/viz/shaders/shader.vert
    src/vulkansift/viz/shaders/shader.frag

    src/vulkansift/sift/shaders/GaussianBlur.comp
    src/vulkansift/sift/shaders/DifferenceOfGaussian.comp
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
    string(APPEND VULKANSIFT_LINKER_OPTIONS "-fno-omit-frame-pointer;-fsanitize=address;")
elseif(VULKANSIFT_SANITIZE STREQUAL Thread)
    string(APPEND VULKANSIFT_COMPILER_OPTIONS "-fno-omit-frame-pointer;-fsanitize=thread;")
    string(APPEND VULKANSIFT_LINKER_OPTIONS "-fno-omit-frame-pointer;-fsanitize=thread;")
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
target_link_libraries(vulkansift
    dl
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
message(${VULKANSIFT_LIB_SHADERS_SPV})
add_dependencies(vulkansift vulkansift_shaders)

#####################################################################################

if(VULKANSIFT_BUILD_EXAMPLES)
message("Examples will be built")

find_package(OpenCV REQUIRED)
# Add test executables
add_executable(test_sift_detect src/examples/test_sift_detect.cpp)

target_link_libraries(test_sift_detect vulkansift ${OpenCV_LIBS})
endif()

if(VULKANSIFT_BUILD_PERFS)
message("Performance tests will be built")
# Add perf executables
endif()

# Copy image resource directory to build folder if examples or perf scripts are built
if(VULKANSIFT_BUILD_EXAMPLES)
add_custom_command(TARGET test_sift_detect POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:test_sift_detect>/res)
#elseif(VULKANSIFT_BUILD_PERFS)
endif()