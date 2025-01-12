from __future__ import division
import sys
sys.path.insert(1,r"D:\移动云网盘\移动云\同步盘-2023-10-24\2024-03\SmartFlow\analyzedCode")

import os
import glob
try:
    from xml.etree import cElementTree as ElementTree
except ImportError:
    from xml.etree import ElementTree
import matplotlib.pyplot as plt
import numpy as np
import json
import ns3XmlParserBase as xmlParser

loadRatioList_5 = ['50', '55', '60', '65', '70', '75', '80', '85', '90', '95', '100']
loadRatioList_10 = ['50', '60', '70', '80', '90', '100']

algNameDict = {'1':'min', '2':'rbnp', '3':'rnd', '4':'lspray', '5':'flet', '6':'ecmp', '7':'rbnf', '8':'rbnh'}
propertyList = [
   "avg_fct",
   "avg_small_fct",
   "avg_large_fct",
   "small_flow_99_fct",
   "flow_99_fct",
   "total_tx",
   "total_rx",
   "flow_count",
   "large_flow_count",
   "large_flow_ratio",
   "min_large_flow_size_in_byte",
   "avg_large_throughput",
   "small_flow_count",
   "small_flow_ratio",
   "max_small_flow_size_in_byte"
]
lineWidth = 4
markerSize = 15

colorDict = {}
colorDict["min"] = 'black'
colorDict["rbnp"] = 'green'
colorDict["rnd"] = 'blue'
colorDict["lspray"] = 'red'
colorDict["flet"] = 'm'
colorDict["ecmp"] = 'darkorange'
colorDict["rbnf"] = 'm'
colorDict["rbnh"] = 'c'
colorDict["realWorld"] = 'red'
colorDict["lsprayr"] = 'c'
colorDict["conga"] = 'skyblue'

colorDict["RPS"] = 'black'
colorDict["ECMP"] = 'green'
colorDict["DRILL"] = 'blue'
colorDict["DEFLOW"] = 'red'
colorDict["LETFLOW"] = 'm'

labelDict = {}
labelDict["min"] = 'MIN'
labelDict["rbnp"] = 'RBPS'
labelDict["rnd"] = 'RPS'
labelDict["lspray"] = 'LAPS'
labelDict["flet"] = 'LetFlow'
labelDict["ecmp"] = 'ECMP'
labelDict["rbnf"] = 'RBNF'
labelDict["rbnh"] = 'RBNH'
labelDict["lsprayr"] = 'LAPS-R'
labelDict["conga"] = 'CONGA'

colorDict["RPS"] = 'RPS'
colorDict["ECMP"] = 'ECMP'
colorDict["DRILL"] = 'DRILL'
colorDict["DEFLOW"] = 'DEFLOW'
colorDict["LETFLOW"] = 'LETFLOW'


markerDict = {}
markerDict["min"] = 'p'
markerDict["rbnp"] = 'o'
markerDict["rnd"] = 'd'
markerDict["lspray"] = 's'
markerDict["flet"] = '>'
markerDict["ecmp"] = "<"
markerDict["rbnf"] = '1'
markerDict["rbnh"] = '2'
markerDict["lsprayr"] = '2'
markerDict["realWorld"] = '-'
markerDict["conga"] = 'D'

markerDict["RPS"] = 'p'
markerDict["ECMP"] = 'o'
markerDict["DRILL"] = 'd'
markerDict["DEFLOW"] = 's'
markerDict["LETFLOW"] = '>'


ylabelDict = {}
ylabelDict["avg_fct"] = 'Total FCT'
ylabelDict["avg_small_fct"] = 'Small Flow FCT'
ylabelDict["avg_large_fct"] = 'Large Flow FCT'
ylabelDict["small_flow_99_fct"] = '99th Small Flow FCT'
ylabelDict["flow_99_fct"] = '99th Flow FCT'
ylabelDict["total_tx"] = 'Tx Bytes'
ylabelDict["total_rx"] = 'Rx Bytes'
ylabelDict["flow_count"] = 'Flow Count'
ylabelDict["large_flow_count"] = 'Large Flow Count'
ylabelDict["large_flow_ratio"] = 'Large Flow Ratio'
ylabelDict["min_large_flow_size_in_byte"] = 'Min Large Flow Size (byte)'
ylabelDict["avg_large_throughput"] = 'Avg Large Flow Throughput'
ylabelDict["small_flow_count"] = 'Small Flow Count'
ylabelDict["small_flow_ratio"] = 'Small Flow Ratio'
ylabelDict["max_small_flow_size_in_byte"] = 'Max Small Flow Size (byte)'

titleDict = {}
titleDict["A00005"] = 'Topology:Dragonfly, Pattern:Reduce'
titleDict["A00003"] = 'Topology:Dragonfly, Pattern:Snake'
titleDict["A00004"] = 'Topology:Dragonfly, Pattern:All2all'
titleDict["A00006"] = 'Dragonfly, Snake, WebSearch'
titleDict["A00007"] = 'Dragonfly, Reduce, WebSearch'
titleDict["A00008"] = 'Dragonfly, All2all, WebSearch'
titleDict["A00009"] = 'Dragonfly, Snake, RPC'
titleDict["A00010"] = 'Dragonfly, Reduce, RPC'
titleDict["A00011"] = 'Dragonfly, All2all, RPC'
titleDict["A00012"] = 'Dragonfly, Snake, VL2'
titleDict["A00013"] = 'Dragonfly, Reduce, VL2'
titleDict["A00014"] = 'Dragonfly, All2all, VL2'
titleDict["A00016"] = 'Fat-tree, Snake, WebSearch'
titleDict["A00017"] = 'Fat-tree, Reduce, WebSearch'
titleDict["A00018"] = 'Fat-tree, All2all, WebSearch'
titleDict["A00019"] = 'Fat-tree, Snake, RPC'
titleDict["A00020"] = 'Fat-tree, Reduce, RPC'
titleDict["A00021"] = 'Fat-tree, All2all, RPC'
titleDict["A00022"] = 'Fat-tree, Snake, DataMining'
titleDict["A00023"] = 'Fat-tree, Reduce, DataMining'
titleDict["A00024"] = 'Fat-tree, All2all, DataMining'
titleDict["A00025"] = 'Fat-tree, Reduce, DataMining,k=6'
titleDict["A00026"] = 'Fat-tree, Reduce, DataMining,reth=10,lunt=1'
titleDict["A00027"] = 'Fat-tree, Reduce, DataMining,reth=5,lunt=1'
titleDict["B00002"] = 'Topology:Dragonfly, Pattern:Reduce'


hatchDict={}
hatchDict['rbnp']='\\',  # 设置柱子内部的图案
hatchDict['rnd']='-',  # 设置柱子内部的图案
hatchDict['lspray']='/',  # 设置柱子内部的图案
hatchDict['conga']='x',  # 设置柱子内部的图案
hatchDict['lsprayr']='x',  # 设置柱子内部的图案


edgecolorDict={}
edgecolorDict['rbnp']='green',  # 设置柱子内部的图案
edgecolorDict['rnd']='blue',  # 设置柱子内部的图案
edgecolorDict['lspray']='red',  # 设置柱子内部的图案
edgecolorDict['conga']='skyblue',  # 设置柱子内部的图案
edgecolorDict['lsprayr']='grey',  # 设置柱子内部的图案


workloadDict = {}
workloadDict["DCTCP_CDF.txt"] = 'WebSearch'
workloadDict["RPC_CDF.txt"] = 'Remote Procedure Call'
workloadDict["VL2_CDF.txt"] = 'Data Mining'


figSize_for_one_figure_one_column = (7,5)

font_for_title_for_one_figure_one_column = {'family': 'serif',
        'color':  'darkred',
        'weight': 'bold',
        'size': 16,    # 字体大小
       }
linewidth_for_one_figure_one_column = 2
markerSize_for_one_figure_one_column = 10

font_size_xticks_for_one_figure_one_column = 28
font_size_yticks_for_one_figure_one_column = 28
font_xlabel_for_one_figure_one_column = {'family' : 'serif',
'weight' : 'bold',
'size'   : 28,
}
font_ylabel_for_one_figure_one_column = {'family' : 'serif',
'weight' : 'bold',
'size'   : 28, #24
}
font_lengend_for_one_figure_one_column = {'family' : 'serif',
'weight' : 'bold',
'size'   : 18,
}

font_lengend_for_test = {'family' : 'serif',
'weight' : 'bold',
'size'   : 8,
}

font_lengend_for_3_1 = {'family' : 'serif',
'weight' : 'bold',
'size'   : 26,
}

font_lengend_for_one_figure_3_column = {'family' : 'serif',
'weight' : 'normal',
'size'   : 8,
}

font_lengend_for_1_figure_2_column_1_raw = {'family' : 'serif',
'weight' : 'bold',
'size'   : 25,
}

def find_the_position_and_value_of_the_first_element_closest_to_target_value_in_list(L, tgt_val):
    position = -1
    value = -1
    if len(L) == 0:
        print("Error in find_the_position_and_value_of_the_first_element_closest_to_target_value_in_list()")
        return position, value
    dif = 10**6
    for idx in range(0, len(L)):
        if abs(tgt_val-L[idx]) < dif:
            dif = abs(tgt_val-L[idx])
            position = idx
    return position, L[position]

def indexing_a_list_by_the_indexes_in_another_list(L1,L2):
    res = []
    for L1_idx in L2:
        if L1_idx >= len(L1):
            print("Error in indexing_a_list_by_the_indexes_in_another_list()")
            res.append(L1[L1_idx])
    return res

def calculate_the_cumsum_of_a_list(originalList):
    length = len(originalList)
    res = [i for i in originalList]
    for i in range(1, len(res)):
        res[i] = res[i-1] + res[i]
    return res

def print_dict_to_file(fileName, d):

    if not isinstance(d, dict):
        print("Error in print_dict_to_file()")

    with open(fileName, "a") as file:  
        for key, value in d.items():  
            file.write(f"{key}: {value}\n")

def print_string_to_file(fileName, string):
    with open(fileName, "a") as file:  
        file.write(string+"\n")


def read_dict_in_json_format(fileName):
    with open(fileName, "r") as infile:
        nested_dict_restored = json.load(infile)
        return nested_dict_restored 

def read_CDF_file(fileName):
    cdf = {"bytes":[], "ratio":[]}
    for data in open(fileName,'r'):
        tmp=data.strip()
        res = tmp.split(' ')
        if len(res) < 2:
            continue
        cdf['bytes'].append(int(res[0]))
        cdf['ratio'].append(float(res[-1]))
    return cdf

def get_cumsum_of_a_list(originalList):
    res = [i for i in originalList]
    for i in range(1, len(res)):
        res[i] = res[i-1] + res[i]
    return res

def get_xml_cdf(fileName):
    data = xmlParser.get_the_flow_status(fileName)
    data["flows"].sort (key=lambda x: x.txBytes)

    x = [flow.txBytes for flow in data["flows"]]
    y = [(j+1)/len(x) for j in range(0, len(x))]
    cumsumFlowBytes = get_cumsum_of_a_list(x)
    sumFlowBytes = sum(x)
    z = [i/sumFlowBytes for i in cumsumFlowBytes]
    print("flowCnt:", len(x))
    data["flowSize"] = x
    data["flowCdf"] = y
    data["byteCdf"] = z
    return data

def get_propotion_of_large_flows(x, ratio):
    s = sum(x)
    id = int(len(x)*(1-ratio))
    ss = sum(x[id:])
    return float(ss*100/s)

def find_the_first_element_smaller_than_given_value_in_list(L, V):
    res = []
    for v in V:
        dif = 10**6
        tmpIdx = -1
        for idx in range(0, len(L)):
            if abs(v-L[idx]) < dif:
                dif = abs(v-L[idx])
                tmpIdx = idx
        res.append(tmpIdx)
    return res
def pick_indexing_elements_from_list(L1,L2):
    res = [-1 for i in L2]
    for idx in range(0, len(L2)):
        res[idx] = L1[L2[idx]]
    return res

def find_target_cdf(x, y, nodeNum=50):
    idle_y = [i/nodeNum for i in range(0,nodeNum+1)]
    idxList = find_the_first_element_smaller_than_given_value_in_list(y, idle_y)
    y = pick_indexing_elements_from_list(y, idxList)
    x = pick_indexing_elements_from_list(x, idxList)
    return x, y

def find_target_cdf_with_given_Y(x, y, idle_y):
    idxList = find_the_first_element_smaller_than_given_value_in_list(y, idle_y)
    y = pick_indexing_elements_from_list(y, idxList)
    x = pick_indexing_elements_from_list(x, idxList)
    return x, y

def average_from_cdf(X, Y):
    assert len(X) == len(Y), "X and Y must have the same length."
    # assert Y[-1] == 1, "The last element of Y must be 1, representing a valid CDF."

    # 计算每个X值对应的期望值贡献
    expected_contributions = [X[i]*(Y[i]-Y[i-1]) for i in range(1, len(X))]
    # 求和所有期望值贡献得到变量的平均值
    avg = round(np.sum(expected_contributions)/1000000,1)
    return avg


def read_Deflow_FCT_file(fileName):
    d = []
    c = 0
    for data in open(fileName,'r'):
        if c < 5:
            tmp=data.strip()
            # print(tmp)
            res = tmp.split()
            print(res)
            if len(res) < 2:
                continue
            d.append([float(res[1]), int(res[0])])
            # cdf['ratio'].append(float(res[-1]))
            c = c + 1
        else:
            break
    print(d)
    return d