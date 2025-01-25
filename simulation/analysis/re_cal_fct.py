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


def parse_txt_file(file_path):
    data = {}
    with open(file_path, 'r') as file:
        isFistLine = True
        lineCount = 0
        for line in file:
            if isFistLine:
                isFistLine = False
                data[lineCount] = line.strip()
                lineCount += 1
                continue
            dataList = line.strip().split()
            if len(dataList) != 7:
                continue
            if line.startswith('//'):
                continue
            key, value = line.split()
            data[key] = float(value) if '.' in value else int(value)
    return data

# 使用示例
file_path = '/d:/GitHubCode/Laps-ns3/simulation/results/C00003/C00003_dragonfly_DCTCP_CDF_All-lr-0.5-lb-plb-QpInfo.txt'
parsed_data = parse_txt_file(file_path)

# 输出解析结果
for key, value in parsed_data.items():
    print(f"{key}: {value}")


