cmake_minimum_required(VERSION 3.0)

find_program(GLSLC_PROG glslc REQUIRED)
if(NOT GLSLC_PROG)
  message(FATAL_ERROR "Could not find the program \"glslc\".")
else()
  message(STATUS "Program \"glslc\" found: ${GLSLC_PROG}")
endif()

find_package(PythonInterp REQUIRED)

function(addSpvShaderFilesDependency FUNC_TARGET ABS_SHADER_PATH_LIST)
  # Compile shaders only if needed (missing/modified) and store them in build/shaders/
  set(SHADERS_SPV "")
  foreach(shader_path ${ABS_SHADER_PATH_LIST})
      get_filename_component(shader_filename ${shader_path} NAME)
      set(output_file ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_filename}.spv)
      add_custom_command(
          OUTPUT ${output_file}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
          COMMAND ${GLSLC_PROG} ${shader_path} -g -o ${output_file}
          DEPENDS ${shader_path}
          COMMENT "Building ${shader_filename}.spv --> ${output_file}"
      )
      message(STATUS "Generating build command for ${shader_filename}.spv")
      set(SHADERS_SPV ${SHADERS_SPV} ${output_file} )
  endforeach()

  add_custom_target(${FUNC_TARGET}_compiled_shaders ALL DEPENDS ${SHADERS_SPV})
  add_dependencies(${FUNC_TARGET} ${FUNC_TARGET}_compiled_shaders)
endfunction()



function(addEmbeddedSpvShaderDependency FUNC_TARGET ABS_SHADER_PATH_LIST OUTPUT_FILE)
  # Create a C file containing the SPIRV code with the vkenv format (see embedded_shader_provider.c.template)
  # Get file path
  get_filename_component(OUTPUT_FILE_DIR ${OUTPUT_FILE} DIRECTORY)

  add_custom_command(
      OUTPUT ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_FILE_DIR} 
      COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_embedded_shader_code.py ${GLSLC_PROG} ${OUTPUT_FILE} ${ABS_SHADER_PATH_LIST} 
      DEPENDS ${ABS_SHADER_PATH_LIST}
  )
  add_custom_target(${FUNC_TARGET}_embedded_shaders ALL DEPENDS ${OUTPUT_FILE})
  add_dependencies(${FUNC_TARGET} ${FUNC_TARGET}_embedded_shaders)
endfunction()