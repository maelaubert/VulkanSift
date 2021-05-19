cmake_minimum_required(VERSION 3.0)

find_program(GLSLC_PROG glslc REQUIRED)
if(NOT GLSLC_PROG)
  message(FATAL_ERROR "Could not find the program \"glslc\".")
else()
  message(STATUS "Program \"glslc\" found: ${GLSLC_PROG}")
endif()

function(addSpvShaderFilesDependency FUNC_TARGET SHADER_PATH_LIST)
  # Compile shaders only if needed (missing/modified) and store them in build/shaders/
  set(SHADERS_SPV "")
  foreach(shader_path ${SHADER_PATH_LIST})
      get_filename_component(shader_filename ${shader_path} NAME)
      set(output_file ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_filename}.spv)
      add_custom_command(
          OUTPUT ${output_file}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
          COMMAND ${GLSLC_PROG} ${CMAKE_CURRENT_SOURCE_DIR}/${shader_path} -g -o ${output_file}
          DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader_path}
          COMMENT "Building ${shader_filename}.spv --> ${output_file}"
      )
      message(STATUS "Generating build command for ${shader_filename}.spv")
      set(SHADERS_SPV ${SHADERS_SPV} ${output_file} )
  endforeach()

  add_custom_target(${FUNC_TARGET}_compiled_shaders ALL DEPENDS ${SHADERS_SPV})
  add_dependencies(${FUNC_TARGET} ${FUNC_TARGET}_compiled_shaders)
endfunction()



function(addEmbeddedSpvShaderDependency FUNC_TARGET SHADER_PATH_LIST OUTPUT_FILE)
  # Create a C file containing the SPIRV code with the vkenv format (see embedded_shader_provider.c.template)
  set(add_shader_code_cmds "")
  set(add_shader_switch_cmds "")
  foreach(shader_path ${VULKANSIFT_LIB_SHADERS})
      get_filename_component(shader_filename ${shader_path} NAME)
      string(REGEX REPLACE "(\\.)" "_" shader_var_name ${shader_filename})
      list(APPEND add_shader_code_cmds COMMAND ${CMAKE_COMMAND} -E echo \"uint32_t ${shader_var_name}[] = \" >> ${OUTPUT_FILE})
      list(APPEND add_shader_code_cmds COMMAND ${GLSLC_PROG} ${CMAKE_CURRENT_SOURCE_DIR}/${shader_path} -mfmt=c -o - >>  ${OUTPUT_FILE})
      list(APPEND add_shader_code_cmds COMMAND ${CMAKE_COMMAND} -E echo "\"\\;\"" >> ${OUTPUT_FILE})

      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo_append "if \\(strcmp\\(shader_path, " >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo_append " \\\"shaders/${shader_filename}.spv\\\" " >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo "\\) == 0\\) \\{" >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo_append "*shader_size = sizeof\\(${shader_var_name}\\)/\\(sizeof\\(uint8_t\\)\\)" >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo "\"\\;\"" >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo_append "*shader_code = \\(const uint8_t*\\)${shader_var_name}" >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo "\"\\;\"" >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo_append "return true" >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo "\"\\;\"" >> ${OUTPUT_FILE})
      list(APPEND add_shader_switch_cmds COMMAND ${CMAKE_COMMAND} -E echo "}" >> ${OUTPUT_FILE})
  endforeach()

  add_custom_command(
      OUTPUT ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E echo "\\#ifdef VKENV_USE_EMBEDDED_SHADERS" > ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E echo "\\#include \\<stdbool.h\\>" >> ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E echo "\\#include \\<stdint.h\\>" >> ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E echo "\\#include \\<string.h\\>" >> ${OUTPUT_FILE}
      ${add_shader_code_cmds}
      COMMAND ${CMAKE_COMMAND} -E echo "bool __vkenv_get_embedded_shader_code\\(const char *shader_path, uint32_t *shader_size, const uint8_t **shader_code\\)" >> ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E echo "{" >> ${OUTPUT_FILE}
      ${add_shader_switch_cmds}
      COMMAND ${CMAKE_COMMAND} -E echo "return false\\;" >> ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E echo "}" >> ${OUTPUT_FILE}
      COMMAND ${CMAKE_COMMAND} -E echo "\\#endif // VKENV_USE_EMBEDDED_SHADERS" >> ${OUTPUT_FILE}
      DEPENDS ${VULKANSIFT_LIB_SHADERS}
  )
  add_custom_target(${FUNC_TARGET}_embedded_shaders ALL DEPENDS ${OUTPUT_FILE})
  add_dependencies(${FUNC_TARGET} ${FUNC_TARGET}_embedded_shaders)

endfunction()
