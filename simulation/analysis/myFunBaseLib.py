import os
import sys
import argparse
import shutil

import os
import sys
import argparse
import shutil
import time

lineWidth = 4
markerSize = 15
grid_linestyle = (0, (10, 3, 2, 3))
font_size_xticks_1_2 = 28
font_size_yticks_1_2 = 28
figureSize_1_2 = (18, 5)


colorDict = {}
colorDict["ecmp"] = 'black'
colorDict["plb"] = 'blue'
colorDict["e2elaps"] = 'red'
colorDict["letflow"] = 'm'
colorDict["conga"] = 'darkorange'
colorDict["conweave"] = 'skyblue'

markerDict = {}
markerDict["conweave"] = 'p'
markerDict["conga"] = 'd'
markerDict["e2elaps"] = 's'
markerDict["letflow"] = 'o'
markerDict["ecmp"] = "<"
markerDict["plb"] = 'D'

labelDict = {}
labelDict["e2elaps"] = 'LAPS'
labelDict["letflow"] = 'LetFlow'
labelDict["ecmp"] = 'ECMP'
labelDict["conga"] = 'CONGA'
labelDict["conweave"] = 'ConWeave'
labelDict["plb"] = 'PLB'

ylabelDict = {}
ylabelDict["avg_fct"] = 'Avg. FCT (ms)'
ylabelDict["large_fct_99"] = 'P99 FCT (ms)'



font_xlabel_1_2 = {'family' : 'serif',
'weight' : 'bold',
'size'   : 28,
}

font_ylabel_1_2 = {'family' : 'serif',
'weight' : 'bold',
'size'   : 28, #24
}

font_lengend_1_2 = {'family' : 'serif',
'weight' : 'bold',
'size'   : 26,
}

def parse_QpInfo_file(fileName):
    res = {}
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
            flowId = int(dataList[1])
            if flowId in res:
                print("Error: duplicated flow id ", flowId, " in file ", fileName)
                return None
            flowSize = float(dataList[2])
            if abs(flowSize) < 1e-6: # if flow size is 0, ignore this flow
                continue
            tmp_res = {}
            for i in range(2, len(dataList)):
                p = propertyList[i]
                tmp_res[p] = float(dataList[i])
            res[flowId] = tmp_res
    sorted_res = sorted(res.items(), key=lambda x: x[1]["FCT"])
    n_flows = len(sorted_res)
    n_small_flows = n_flows // 2
    small_fct = sum(x[1]["FCT"] for x in sorted_res[:n_small_flows]) / n_small_flows/1000
    n_large_flows = n_flows//5
    large_fct = sum(x[1]["FCT"] for x in sorted_res[-n_large_flows:]) / n_large_flows/1000
    index_small_flow_99 = n_small_flows*99//100
    small_fct_99 = sorted_res[index_small_flow_99][1]["FCT"]/1000
    index_large_flow_99 = n_flows*99//100
    large_fct_99 = sorted_res[index_large_flow_99][1]["FCT"]/1000
    toal_fct = sum(x[1]["FCT"] for x in sorted_res)/n_flows/1000
    return n_flows, small_fct, large_fct, small_fct_99, large_fct_99, toal_fct




