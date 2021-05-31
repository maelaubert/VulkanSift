import sys
import os
import matplotlib.pyplot as plt

detector_names = ["VLFeat", "OpenCV", "SiftGPU", "PopSift", "VulkanSift"]
set_names = ["bark","bikes","boat","graf","leuven","trees","ubc","wall"]
dataset_path = "../../Dataset/feature_evaluation_data/"

detector_name = "OpenCV"

img_list = []

for set_n in set_names:
  files_path = dataset_path+"/"+set_n+"/"
  files = os.listdir(files_path)
  for f in files:
    if(f.endswith(".ppm") or f.endswith(".pgm")):
      img_list.append(files_path+f)
      print(files_path+f)

print(img_list)

sum_ms = 0.0

meas_list =[]

out_file = open("oxford_runtime_points_"+detector_name+".txt","w")

for img_path in img_list:
  os.system("../build/perf_sift_runtime "+img_path+ " "+detector_name)
  rt_meas_file = open("../build/runtime_results_"+detector_name+".txt")
  line = rt_meas_file.readline()
  meas_ms = float(line.split(";")[0])
  nb_feats = int(line.split(";")[1])
  sum_ms += meas_ms
  meas_list.append(meas_ms)
  print(len(meas_list), meas_list)
  rt_meas_file.close()
  out_file.write(str(meas_ms)+";"+str(nb_feats)+"\n")

print(sum_ms/len(img_list))

out_file.close()