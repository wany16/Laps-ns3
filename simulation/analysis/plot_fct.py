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

import decimal

# 设置全局浮点数精度和格式
decimal.getcontext().prec = 2

index = ["C00003"]
topo = ["dragonfly", "railOnly"]
workload = ["RPC_CDF", "AliStorage2019", "DCTCP_CDF", "FdHdp2015", "GoogleRPC2008", "VL2_CDF"]
pattern = ["ALL"]
algName = ["e2elaps", "plb", "ecmp", "letflow", "conweave", "conga"]
loadratio = ["0.5", "0.55", "0.6", "0.65", "0.7", "0.75", "0.8", "0.85", "0.9", "0.95", "1.0"]
rootDir = "../results/"
for idx in index:
    for tp in topo[1]:
        for wl in workload[0]:
            for ptn in pattern[0]:
                for alg in algName[:-1]:
                    for lr in loadratio:
                            fileName = idx + "_" + tp + "_" + wl + "_" + ptn + "-lr-" + lr + "-lb-" + alg + "-QpInfo.txt"
                            inFile = rootDir + alg + "/" + fileName
                            if not os.path.exists(inFile):
                                print("Error: ", inFile, " does not exist")
                                continue
                            # print("Processing ", inFile)
                            n_flows, small_fct, large_fct, small_fct_99, large_fct_99, avg_fct = mylib.parse_QpInfo_file(inFile)
                            print(
                                  "alg=", alg,
                                  "topo=", tp,
                                  "workload=", wl,
                                  "pattern=", ptn,
                                  "n_flows=", n_flows,
                                  "avg_fct=", avg_fct,
                                  "small_fct=", small_fct,
                                  "large_fct=", large_fct,
                                  "small_fct_99=", small_fct_99,
                                  "large_fct_99=", large_fct_99
                                  )
                        


