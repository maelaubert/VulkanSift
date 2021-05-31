import sys
import os
import matplotlib.pyplot as plt

detector_names = ["VLFeat", "OpenCV", "SiftGPU", "PopSift", "VulkanSift"]
set_names = ["colors","grace","posters","there","underground"]
dataset_path = "../../Dataset/hannover/"

detector_name = "SiftGPU"
resolution = "3456x2304"
#resolution = "1536x1024"

img_list = []

for set_n in set_names:
  files_path = dataset_path+"/"+set_n+"_"+resolution+"/"
  files = os.listdir(files_path)
  for f in files:
    if(f.endswith(".ppm")):
      img_list.append(files_path+f)
      print(files_path+f)

print(img_list)

sum_ms = 0.0

meas_list =[]

for img_path in img_list:
  os.system("../build/perf_sift_runtime "+img_path+ " "+detector_name)
  rt_meas_file = open("../build/runtime_results_"+detector_name+".txt")
  meas_ms = float(rt_meas_file.readline().split(";")[0])
  sum_ms += meas_ms
  meas_list.append(meas_ms)
  print(len(meas_list), meas_list)
  rt_meas_file.close()

print(sum_ms/len(img_list))
