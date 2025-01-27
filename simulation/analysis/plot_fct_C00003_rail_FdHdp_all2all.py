import os
import sys
import argparse
import shutil

import os
import sys
import argparse
import shutil
import time
import myFunBaseLib as mylib
import matplotlib.pyplot as plt

import decimal

# 设置全局浮点数精度和格式
decimal.getcontext().prec = 2

idx = "C00003"
topo = ["dragonfly", "railOnly"]
tp = topo[1]
workload = ["RPC_CDF", "AliStorage2019", "DCTCP_CDF", "FbHdp2015", "GoogleRPC2008", "VL2_CDF"]
wl = workload[3]

ptn = "ALL"
# algName = ["e2elaps", "plb", "ecmp", "letflow", "conweave", "conga"]
algName = ["e2elaps", "plb", "ecmp", "letflow", "conweave", "conga"]
loadratio = ["0.5", "0.55", "0.6", "0.65", "0.7", "0.75", "0.8", "0.85", "0.9", "0.95", "1.0"]
inRootDir = "../results/"
outRootDir = "../Figures/"
data = {}
for alg in algName:
    for lr in loadratio:
        fileName = idx + "_" + tp + "_" + wl + "_" + ptn + "-lr-" + lr + "-lb-" + alg + "-QpInfo.txt"
        inFile = inRootDir + alg + "/" + fileName
        if not os.path.exists(inFile):
            print("Error: ", inFile, " does not exist")
            continue
        # print("Processing ", inFile)
        n_flows, small_fct, large_fct, small_fct_99, large_fct_99, avg_fct\
        = mylib.parse_QpInfo_file(inFile)
        if alg not in data:
            data[alg] = {}
        data[alg][lr] =\
            {"n_flows": n_flows,\
              "small_fct": small_fct,\
              "large_fct": large_fct,\
              "small_fct_99": small_fct_99,\
              "large_fct_99": large_fct_99,\
              "avg_fct": avg_fct\
            }                        # print(
            #       "alg=", alg,
            #       "topo=", tp,
            #       "workload=", wl,
            #       "pattern=", ptn,
            #       "n_flows=", n_flows,
            #       "avg_fct=", avg_fct,
            #       "small_fct=", small_fct,
            #       "large_fct=", large_fct,
            #       "small_fct_99=", small_fct_99,
            #       "large_fct_99=", large_fct_99
            #       )
                        
fig, axs = plt.subplots(1, 2, figsize=mylib.figureSize_1_2)
plotIdx = 0
x_list = [int(float(x)*100) for x in loadratio]
propertyList = ["avg_fct", "large_fct_99"]
for propertyIdx in range(0, len(propertyList)):
    prpt = propertyList[propertyIdx]
    for alg in algName:
        y_list = []
        for lr in loadratio:
            yVal = data[alg][lr][prpt]
            y_list.append(round(yVal, 6))
        y_list.sort()
        print(alg, y_list)
        if plotIdx == 0:
            axs[plotIdx].plot(x_list, y_list,
                              linewidth=mylib.lineWidth,
                              color=mylib.colorDict[alg],
                              label=mylib.labelDict[alg],
                              marker=mylib.markerDict[alg],
                              # markerfacecolor='none',  # 标记点为空心
                              markersize=mylib.markerSize)
        else:
            axs[plotIdx].plot(x_list, y_list,
                              linewidth=mylib.lineWidth,
                              color=mylib.colorDict[alg],
                              marker=mylib.markerDict[alg],
                              # markerfacecolor='none',  # 标记点为空心
                              markersize=mylib.markerSize)
            
    axs[plotIdx].set_xlabel('Load Ratio (%)', mylib.font_xlabel_1_2)
    axs[plotIdx].set_ylabel(mylib.ylabelDict[prpt], mylib.font_ylabel_1_2)
    axs[plotIdx].grid(True,linestyle = mylib.grid_linestyle)
    axs[plotIdx].tick_params(axis='x', labelsize=mylib.font_size_xticks_1_2)
    axs[plotIdx].tick_params(axis='y', labelsize=mylib.font_size_yticks_1_2)
    axs[plotIdx].set_xticks(x_list[::2], [str(x) for x in x_list[::2]])
    plotIdx = plotIdx + 1

# plt.yscale('symlog', linthresh=1)
# plt.yscale('symlog')
# y_pticks_0 = [1, 2, 3, 4]
# axs[0].set_yticks(y_pticks_0, [str(x) for x in y_pticks_0])
# axs[0].set_ylim([0, 5])  # 这里设置 y 轴的最小值和最大值
#
# y_pticks_1 = [0, 50, 100, 150]
# axs[1].set_yticks(y_pticks_1, [str(x) for x in y_pticks_1])
# axs[1].set_ylim([0, 200])  # 这里设置 y 轴的最小值和最大值

fig.legend(prop=mylib.font_lengend_1_2,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=6,
        handlelength=2,  # 调整图例项的长度
        columnspacing=1.0,  # 调整图例列之间的间隔
        edgecolor = 'black')

fig.tight_layout()
outputFile = outRootDir + idx + "-" + tp + "-" + wl + "-" + ptn + ".pdf"
fig.savefig(outputFile, bbox_inches='tight')
plt.show()



