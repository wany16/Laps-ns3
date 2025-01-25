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
    propertyList = []
    with open(fileName, 'r') as file:
        isFistLine = True
        for line in file:
            if isFistLine:
                isFistLine = False
                propertyList = line.strip().split()
                continue
            dataList = line.strip().split()
            if len(dataList) != len(propertyList):
                continue
            if int(dataList[0]) in dataDict:
                print("Error: duplicated flow id")
                return None
            dataDict[int(dataList[0])] = {}
            for i in range(1, len(dataList)):
                dataList[int(dataList[0])][propertyList[i]] = float(dataList[i])
    return dataDict




