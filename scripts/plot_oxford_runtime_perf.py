import sys
import os
import matplotlib.pyplot as plt
import numpy as np

res_files = [n for n in os.listdir(".") if "oxford_runtime_points_" in n]
print(res_files)
results = {}

for res_file_name in res_files:
    detector_name = res_file_name.split("oxford_runtime_points_")[1].split(".txt")[0]

    print("Detector name: ", detector_name)

    res_file = open(res_file_name, "r")

    runtime_ms = []
    nb_feats = []

    for line in res_file:
        if not detector_name in results.keys():
            results[detector_name] = {"meas_ms":[], "nb_feats": []}
            
        meas_ms = float(line.split(";")[0])
        nb_feat = int(line.split(";")[1])

        results[detector_name]["meas_ms"].append(meas_ms)
        results[detector_name]["nb_feats"].append(nb_feat)


# Save figures
for det_name in results.keys():
  # sort arrays by number of features
  order_arr = np.argsort(results[det_name]["nb_feats"])
  xs = np.array(results[det_name]["nb_feats"])[order_arr]
  ys = np.array(results[det_name]["meas_ms"])[order_arr]
  plt.plot(xs,ys, label=det_name)
  plt.xlabel("Number of detected features")
  plt.ylabel("Detection time (ms)")
  #plt.ylim(0,100)
plt.legend()
plt.savefig("perf_runtime_oxford_640x480.png",)
plt.clf()
#plt.show()