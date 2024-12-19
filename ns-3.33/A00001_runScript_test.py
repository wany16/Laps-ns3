import os
import sys
import argparse
import shutil

#define HIGHEST_PRIORITY_FIRST_STRATEGY 1
#define ROUND_ROBIN_STRATEGY 2
#define RANDOM_STRATEGY 3
#define SMALL_LATENCY_FIRST_STRATEGY 4
#define FLOWLET_STATEGY 5
#define FLOW_HASH_STRATEGY 6


parser = argparse.ArgumentParser(description="请输入参数：(1)srcNodeIdx (2)dstNodeIdx (3)sendingPktNum (4)sendingIntervalInNs (5)pathSelStrategy (6)pathExpiredTimeThldInNs")
parser.add_argument("--srcNodeIdx", default="36", help="测试时发送端节点的序号, 默认值36为第1台服务器")
parser.add_argument("--dstNodeIdx", default="38", help="测试时接收端节点的序号, 默认值38为第3台服务器")
parser.add_argument("--sendingPktNum", default="2", help="测试时发送报文数量, 默认值2")
parser.add_argument("--sendingIntervalInNs", default="1000000", help="测试时发送报文时间间隔(单位：纳秒), 默认值1-000-000")
parser.add_argument("--pathSelStrategy", default="4", help=r"{1:min}, {2:rbn}, {3:rnd}, {4:lspray}, {5:flet}, {6:ecmp}, 默认值4")
parser.add_argument("--pathExpiredTimeThldInNs", default="1000000", help=r"路径过期时间(时间：纳秒), 默认值1-000-000")
args = parser.parse_args()

root_dir_predix = "/file-in-ctr/"
inputFile_dir_prefix = "inputFiles/"
outputFile_dir_prefix = "outputFiles/"
executableFile_dir_prefix = "executableFiles/"
workload_dir_prefix = "workLoad/"
mainFile_file_prefix = "A00001-"
scratch_dir_base = "/app/ns3-detnet-rdma-main/ns-3.33/scratch/"
waf_dir_base = "/app/ns3-detnet-rdma-main/ns-3.33/"

mainFile      =                                           mainFile_file_prefix + "MAIN-test"
if os.path.exists(scratch_dir_base + mainFile + ".cc"):
    os.remove(scratch_dir_base + mainFile + ".cc")
shutil.copy(root_dir_predix + executableFile_dir_prefix + mainFile + ".cc", scratch_dir_base)
os.chdir(waf_dir_base)

addrFile      = root_dir_predix + inputFile_dir_prefix  + mainFile_file_prefix + "ADDR.txt" 
vmtFile       = root_dir_predix + inputFile_dir_prefix  + mainFile_file_prefix + "VMT.txt" 
pstFile       = root_dir_predix + inputFile_dir_prefix  + mainFile_file_prefix + "PST.txt" 
pitFile       = root_dir_predix + inputFile_dir_prefix  + mainFile_file_prefix + "PIT.txt" 
tfcFile       = root_dir_predix + inputFile_dir_prefix  + mainFile_file_prefix + "TFC-S.txt" 
topoFile      = root_dir_predix + inputFile_dir_prefix  + mainFile_file_prefix + "TOPO.txt" 
chlFile       = root_dir_predix + inputFile_dir_prefix  + mainFile_file_prefix + "CHL.txt" 
workloadFile  = root_dir_predix + workload_dir_prefix                          + "DCTCP_CDF.txt"

monitorFile   = root_dir_predix + outputFile_dir_prefix + mainFile_file_prefix + "flows.xml"
parameterFile = root_dir_predix + outputFile_dir_prefix + mainFile_file_prefix + "paras.txt"
probeFile     = root_dir_predix + outputFile_dir_prefix + mainFile_file_prefix + "probe.txt"

simStartInSec = 0.0
simEndInSec = 1.5
flowLaunchEndInSec = 1.5
loadFactor = 1.0

Line_command = '\
        ./waf --run "scratch/{}\
        --addrFile={} \
        --vmtFile={} \
        --pstFile={} \
        --pitFile={} \
        --tfcFile={} \
        --topoFile={} \
        --chlFile={} \
        --workloadFile={} \
        --monitorFile={} \
        --parameterFile={} \
        --probeFile={}\
        --loadFactor={}\
        --simStartInSec={} \
        --simEndInSec={} \
        --flowLaunchEndInSec={} \
        --srcNodeIdx={} \
        --dstNodeIdx={} \
        --sendingPktNum={} \
        --sendingIntervalInNs={} \
        --pathSelStrategy={} \
        --pathExpiredTimeThldInNs={} \
        "\
        '.format(\
        mainFile,\
        addrFile,\
        vmtFile,\
        pstFile,\
        pitFile,\
        tfcFile,\
        topoFile,\
        chlFile,\
        workloadFile,\
        monitorFile,\
        parameterFile,\
        probeFile,\
        loadFactor,\
        simStartInSec,\
        simEndInSec,\
        flowLaunchEndInSec,\
        args.srcNodeIdx,\
        args.dstNodeIdx,\
        args.sendingPktNum,\
        args.sendingIntervalInNs,\
        args.pathSelStrategy,\
        args.pathExpiredTimeThldInNs\
        )
print(Line_command)
os.system(Line_command)



