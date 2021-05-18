import numpy as np
import sys
import os
import matplotlib.pyplot as plt

res_files = [n for n in os.listdir(".") if "matching_results" in n]
print(res_files)

results = {}

for res_file_name in res_files:
    detector_name = res_file_name.split("matching_results_")[
        1].split(".txt")[0]

    print("Detector name: ", detector_name)

    res_file = open(res_file_name, "r")

    pmr_arr = []
    precision_arr = []

    for line in res_file:
        dataset_name = line.split(";")[0]

        if not dataset_name in results.keys():
            results[dataset_name] = {}
        if not detector_name in results[dataset_name].keys():
            results[dataset_name][detector_name] = {"Putative Match Ratio": [], "Precision": [], "Matching score": []}

        pmr = float(line.split(";")[3])*100
        precision = float(line.split(";")[4])*100
        matching_score = float(line.split(";")[5])*100

        results[dataset_name][detector_name]["Putative Match Ratio"].append(pmr)
        results[dataset_name][detector_name]["Precision"].append(precision)
        results[dataset_name][detector_name]["Matching score"].append(matching_score)

# Save figures
for dataset_name in results.keys():
  for metric_name in results[dataset_name][list(results[dataset_name].keys())[0]].keys():
    for det_name in results[dataset_name].keys():
      plt.plot([n+1 for n in range(0, len(results[dataset_name][det_name][metric_name]))],results[dataset_name][det_name][metric_name], label=det_name)
      plt.xticks([n+1 for n in range(0, len(results[dataset_name][det_name][metric_name]))])
      plt.xlabel("Image pair")
      plt.ylabel(metric_name)
      plt.ylim(0,100)
    plt.legend()
    plt.savefig(dataset_name+"_"+metric_name+".png",)
    plt.clf()
    #plt.show()
