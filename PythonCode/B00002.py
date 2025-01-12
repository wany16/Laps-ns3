from __future__ import division
import sys
import os
import glob
try:
    from xml.etree import cElementTree as ElementTree
except ImportError:
    from xml.etree import ElementTree
import matplotlib.pyplot as plt
import numpy as np
import json
import plottingBase as base

experimentalName = "B00002"
src_file_path = "D:/移动云网盘/移动云/同步盘-2023-10-24/2024-07/DeFlow-for-Computer-Networks/compressedFiles/" + experimentalName + "/" + experimentalName + "-"


outputFileDir = "../figure/"
if not os.path.exists(outputFileDir):
    os.makedirs(outputFileDir)



targetPropertyIdx = 0
loadrationIdx = -5
preciseWidth = 1
baseAlg = 'lspray'
res = {}

algorithm = [ 'DEFLOW', 'ECMP', 'DRILL', 'LETFLOW', 'RPS']
algorithm = [ 'DEFLOW', 'ECMP', 'LETFLOW', 'RPS']
loadRatio = ['50', '55', '60', '65', '70', '75', '80', '85', '90', '95', '100']
xList = [50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100]
propertyList = [
   "avg_fct",
   "avg_small_fct",
   "small_flow_99_fct",
   "avg_large_fct",
   "large_flow_99_fct"
   ]


if __name__ == "__main__":
    fig, axs = plt.subplots(1, 5, figsize=(35, 5))
    plotIdx = 0
    for propertyIdx in range(0, len(propertyList)):
        for alg in algorithm:
            srcFile = src_file_path + alg + "/" + alg + "-"
            yList = []
            for lr in loadRatio:
                srcFile = srcFile + lr + "-FCT.txt"
                yVal = base.read_Deflow_FCT_file(srcFile)[propertyIdx][0]/1000
                yList.append(yVal)
            yList.sort()
            if plotIdx == 0:
                axs[plotIdx].plot(xList, yList,
                                  linewidth=base.lineWidth,
                                  color=base.colorDict[alg],
                                  label=base.labelDict[alg],
                                  marker=base.markerDict[alg],
                                  # markerfacecolor='none',  # 标记点为空心
                                  markersize=base.markerSize)
            else:
                axs[plotIdx].plot(xList, yList,
                                  linewidth=base.lineWidth,
                                  color=base.colorDict[alg],
                                  marker=base.markerDict[alg],
                                  # markerfacecolor='none',  # 标记点为空心
                                  markersize=base.markerSize)
        axs[plotIdx].set_xlabel('Load Ratio (%)', base.font_xlabel_for_one_figure_one_column)
        axs[plotIdx].set_ylabel(base.ylabelDict[propertyList[propertyIdx]], base.font_ylabel_for_one_figure_one_column)
        axs[plotIdx].grid(True,linestyle = (0, (10, 3, 2, 3)))
        axs[plotIdx].tick_params(axis='x', labelsize=base.font_size_xticks_for_one_figure_one_column)
        axs[plotIdx].tick_params(axis='y', labelsize=base.font_size_yticks_for_one_figure_one_column)
        axs[plotIdx].set_xticks([50, 60, 70, 80, 90, 100],['50','60', '70', '80', '90','100'])

        plotIdx = plotIdx + 1
    
    fig.legend(prop=base.font_lengend_for_3_1,
            loc='upper center',
            bbox_to_anchor=(0.5, 1.18),
            ncol=7,
            handlelength=3,  # 调整图例项的长度
            columnspacing=3.5,  # 调整图例列之间的间隔
            edgecolor = 'black')

    fig.tight_layout()
    outputFile = outputFileDir\
                + experimentalName\
                + ".pdf"
    fig.savefig(outputFile, bbox_inches='tight')
    fig.show()

    # print(baseAlg,
    #       propertyList[targetPropertyIdx],
    #       loadRatio[loadrationIdx],
    #       np.round(res[baseAlg][propertyList[targetPropertyIdx]][loadrationIdx],1))
    # for alg in res.keys():
    #     if alg != baseAlg:
    #         print(alg,
    #               propertyList[targetPropertyIdx],
    #               loadRatio[loadrationIdx],
    #               np.round(res[alg][propertyList[targetPropertyIdx]][loadrationIdx]/
    #                        res[baseAlg][propertyList[targetPropertyIdx]][loadrationIdx],1))
