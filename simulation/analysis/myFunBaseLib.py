import os
import sys
import argparse
import shutil

import os
import sys
import argparse
import shutil
import time
"""copy to physical server flie address: home/ying/file-in-host"""


def parse_QpInfo_file(fileName):
    dataDict = {}
    with open(fileName, 'r') as file:
        isFistLine = True
        lineCount = 0
        for line in file:
            if isFistLine:
                isFistLine = False
                continue
            dataList = line.strip().split()
            if len(dataList) != 7:
                continue
            if int(dataList[0]) in dataDict:
                print("Error: duplicated flow id")
                return None
            dataDict[int(dataList[0])] = {"FlowId":int(dataList[1]),\
                                          "Size":float(dataList[2]),\
                                          "Sent":float(dataList[3]),\
                                          "Recv":float(dataList[4]),\
                                          "FCT":float(dataList[5]),\
                                          "PFC":float(dataList[6])}
    return dataDict

# 使用示例
file_path = '/d:/GitHubCode/Laps-ns3/simulation/results/C00003/C00003_dragonfly_DCTCP_CDF_All-lr-0.5-lb-plb-QpInfo.txt'
parsed_data = parse_txt_file(file_path)

# 输出解析结果
for key, value in parsed_data.items():
    print(f"{key}: {value}")


