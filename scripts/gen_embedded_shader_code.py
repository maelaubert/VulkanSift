import os
import sys
import subprocess

print("C embedded shader code generation")

# First param is the glslc compiler path
glsl_path = sys.argv[1]
print("GLSLC path:", glsl_path)

if(len(sys.argv)<=3):
  print("Error: list of SPV file to embed is empty")

target_output_filepath = sys.argv[2]
print("Output C file path:", target_output_filepath)

spv_name_data_dict = {}

for i in range(3, len(sys.argv)):
  spv_file_path = sys.argv[i]
  if not os.path.exists(spv_file_path):
    print("Error: SPV file "+spv_file_path+" does not exists")
    exit(-1)
  spv_filename = os.path.basename(spv_file_path)
  #print("\t ",spv_file_path)
  #print("\t ",os.path.basename(spv_file_path))
  # Compile shader code with glslc
  spv_name_data_dict[spv_filename] = subprocess.check_output([glsl_path, spv_file_path, "-mfmt=c", "-o", "-"]).decode("utf-8").replace("\n","")

output_file =  open(target_output_filepath, "w")

# Write header
output_file.write("""
#ifdef VKENV_USE_EMBEDDED_SHADERS

#include "vulkan_utils.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

""")
# Write shader_code_variables
for shader_name, shader_formatted_code in spv_name_data_dict.items():
  output_file.write("uint32_t "+shader_name.replace(".","_")+"[] = "+str(shader_formatted_code)+";\n")
# Write start of shader selection function
output_file.write("""

bool __vkenv_get_embedded_shader_code(const char *shader_path, uint32_t *shader_size, const uint8_t **shader_code)
{
""")
# Write spv code provider for each shader
for shader_name, shader_formatted_code in spv_name_data_dict.items():
  output_file.write("\t if (strcmp(shader_path, \"shaders/"+shader_name+".spv\") == 0)\n")
  output_file.write("\t {\n")
  output_file.write("\t\t *shader_size = sizeof("+shader_name.replace(".","_")+")/(sizeof(uint8_t));\n")
  output_file.write("\t\t *shader_code = (const uint8_t*)"+shader_name.replace(".","_")+";\n")
  output_file.write("\t\t return true;\n")
  output_file.write("\t }\n")
# Write end of the file
output_file.write("""
  else
  {
    return false;
  }
}
#endif""")
output_file.close()

print("Code generation completed")