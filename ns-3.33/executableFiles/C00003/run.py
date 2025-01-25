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


def copy_newer_files(src_dir, dst_dir):
    # 确保目标目录存在
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)

    # 使用os.walk遍历源目录及其子目录
    # root：当前目录的路径（字符串形式）。dirs：当前目录下子目录的名称列表（字符串列表）。files：当前目录下文件的名称列表（字符串列表）。
    for root, dirs, files in os.walk(src_dir):
        # 计算相对于源目录的相对路径
        rel_root = os.path.relpath(root, src_dir)
        dst_subdir = os.path.join(dst_dir, rel_root)
        # 确保目标目录的子目录存在
        if not os.path.exists(dst_subdir):
            os.makedirs(dst_subdir)
        # 遍历文件
        for file in files:
            src_file = os.path.join(root, file)
            dst_file = os.path.join(dst_subdir, file)
            # 如果目标文件不存在或源文件更新时间晚于目标文件，则复制源文件到目标目录
            if not os.path.exists(dst_file) or os.path.getmtime(
                    src_file) > os.path.getmtime(dst_file):
                shutil.copy2(src_file, dst_subdir)
                print(
                    f"File {src_file} is newer or does not exist in destination, copied to {dst_subdir}."
                )


def override_files(src_dir, dst_dir):
    # 确保目标目录存在
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)

    # 使用os.walk遍历源目录及其子目录
    # root：当前目录的路径（字符串形式）。dirs：当前目录下子目录的名称列表（字符串列表）。files：当前目录下文件的名称列表（字符串列表）。
    for root, dirs, files in os.walk(src_dir):
        # 计算相对于源目录的相对路径
        rel_root = os.path.relpath(root, src_dir)
        dst_subdir = os.path.join(dst_dir, rel_root)
        # 确保目标目录的子目录存在
        if not os.path.exists(dst_subdir):
            os.makedirs(dst_subdir)
        # 遍历文件
        for file in files:
            src_file = os.path.join(root, file)
            dst_file = os.path.join(dst_subdir, file)
            # 如果目标文件不存在或源文件更新时间晚于目标文件，则复制源文件到目标目录
            if os.path.exists(dst_file):
                os.remove(dst_file)  # 递归删除文件夹下的所有子文件夹和子文件和其本身
                print(f"File {dst_file} is deleted.")

            shutil.copy2(src_file, dst_subdir)
            print(f"File {src_file} is copied to {dst_subdir}.")


vm_root_path = "/file-in-ctr/" if os.path.exists(
    "/file-in-ctr/") else "/file-in-cntr/"
ns3_root_path = "/app/ns3-detnet-rdma-main/ns-3.33/"

# update the module files
sydDirList = ['src']
for sydDir in sydDirList:
    override_files(vm_root_path + sydDir, ns3_root_path + sydDir)

current_file_path = os.path.abspath(__file__)
current_dir_path = os.path.dirname(current_file_path)
current_dir_name = os.path.basename(current_dir_path)
experimentalName = current_dir_name
mainFileName = "main"

parser = argparse.ArgumentParser(description="请输入以下参数")
parser.add_argument("--configFileName",
                    default="CONFIG_DCQCN.txt",
                    help="defaultFileName, by default CONFIG.txt")
parser.add_argument("--topoFileDir",
                    default="test",
                    help="default TOPO File Name, by test")
parser.add_argument("--topoFileName",
                    default="TOPO.txt",
                    help="defaultFileName, by default TOPO_S5_H4.txt/fat_tree_4-8-8-16_topology.txt")
parser.add_argument("--pitFileName",
                    default="PIT.txt",
                    help="defaultFileName, by default PIT_S5_H4_L10.txt/fat_tree_topology_PIT.txt")
parser.add_argument("--pstFileName",
                    default="PST.txt",
                    help="defaultFileName, by default PST_S5_H4_L10.txt/fat_tree_topology_PST.txt")
parser.add_argument("--smtFileName",
                    default="SMT.txt",
                    help="defaultFileName, by default fat_tree_topology_SMT.txt")

parser.add_argument("--simStartTimeInSec",
                    default="0",
                    help="simulation start time")
parser.add_argument("--simEndTimeInSec",
                    default="15",
                    help="simulation end time")
parser.add_argument("--flowLunchStartTimeInSec",
                    default="0.01",
                    help="flow start time")
parser.add_argument("--flowLunchEndTimeInSec",
                    default="0.01",
                    help="flow end time")
parser.add_argument("--qlenMonitorIntervalInNs",
                    default="10000000",
                    help="Qlen Monitor period In Ns")
parser.add_argument("--lbsName",
                    default="RPS",
                    help="Load balancing algorithm")
parser.add_argument("--flowletTimoutInUs",
                    default="10",
                    help="The time out of the flowlet in microsecond.")
parser.add_argument(
    "--loadRatioShift",
    default="1.0",
    help="loadfactorAdjustFacror:Ring ->1,all2all->1/(n-1),Reduce->1/(K-1),n is host num,k is group num."
)
parser.add_argument(
    "--PS",
    default="30",
    help="Physical Server 30 is DCTCP_CDF',29 is RPC_CDF,28 is VL2_CDF")
parser.add_argument("--ccMode", default="Dcqcn_mlx",  help="congestion control algorithm")
parser.add_argument("--screenDisplayInNs", default="10000000",  help="screen display interval in Ns")
parser.add_argument("--enablePfcMonitor", default="false",  help="trace Pfc packets or not ")
parser.add_argument("--enableFctMonitor", default="true",  help="trace Fct or not")
parser.add_argument("--enableQlenMonitor", default="false",  help="trace queue length or not")
parser.add_argument("--rdmaAppStartPort", default="1000",  help="minimal port for rdma client")
parser.add_argument("--enableQbbTrace", default="false",  help="trace the packet event on node's all Qbb netdevices")
parser.add_argument("--testPktNum", default="1",  help="The number of packets to test")
parser.add_argument("--enableLLMWorKLoad", default="false",  help="The LLM work load test")
args = parser.parse_args()

vm_root_path = "/file-in-ctr/"
if not os.path.exists(vm_root_path):
    vm_root_path = "/file-in-cntr/"

# VM:specify the input files
vm_inputFiles_path = vm_root_path + "inputFiles/" + experimentalName + "/"
vm_workload_path = vm_root_path + "inputFiles/" + "workload/"


#PhysicalServer 2 lb





vm_patternFiles_path = vm_root_path + "inputFiles/" + "pattern/"
# patternName = 'Ring'

# VM:specify the exec files
vm_executable_path = vm_root_path + "executableFiles/" + experimentalName + "/"
vm_mainFile_path = vm_executable_path + mainFileName + ".cc"
vm_userdefinedfunction_path = vm_root_path + "userdefinedfunction/"
vm_smartflow_path = vm_root_path + "smartflow-routing/"
vm_userdefinedfunction_model_path = vm_userdefinedfunction_path + "model/"
vm_smartflow_model_path = vm_smartflow_path + "model/"
# VM:specify the output files
vm_outputFiles_path = vm_root_path + "outputFiles/" + experimentalName + "/"

ns3_base_path = "/app/ns3-detnet-rdma-main/ns-3.33/"
ns3_smartflow_path = ns3_base_path + "src/smartflow-routing/"
ns3_userdefinedfunction_path = ns3_base_path + "src/userdefinedfunction/"
ns3_scratch_path = "/app/ns3-detnet-rdma-main/ns-3.33/scratch/"
ns3_mainFile_path = ns3_scratch_path + mainFileName + ".cc"
ns3_userdefinedfunction_model_path = ns3_userdefinedfunction_path + "model/"
ns3_smartflow_model_path = ns3_smartflow_path + "model/"

ns3_waf_path = "/app/ns3-detnet-rdma-main/ns-3.33/"

# update the main.cc
if os.path.exists(ns3_mainFile_path):
    os.remove(ns3_mainFile_path)
shutil.copy(vm_mainFile_path, ns3_scratch_path)

# create the sub dir for the results in outputFiles/
if not os.path.exists(vm_outputFiles_path):
    # shutil.rmtree(vm_outputFiles_path)   #递归删除文件夹下的所有子文件夹和子文件
    os.makedirs(vm_outputFiles_path)
# file_path = os.path.join(dir_path, 'A.txt')  # 文件路径

os.chdir(ns3_waf_path)
allLoadratioList = [
    '0.5', '0.55', '0.6', '0.65', '0.7', '0.75', '0.8', '0.85', '0.9', '0.95',
    '1.0'
]
m_PS2lb={'30':'ecmp','29':'letflow','28':'conga','27':'conweave','26':'plb','25':'e2elaps'}
#patternNames = ['Ring', 'all2all', 'Reduce']
patternNameMap = {'Ring': 1, 'All': 0.032, 'Reduce': 0.333}
onePatternNameMap = {'All': 1}
allLbsNameList = ['drill', 'letflow', 'ecmp','laps','conweave','conga']
loadratioListall=[
    '0.5', '0.55', '0.6', '0.65', '0.7', '0.75', '0.8', '0.85', '0.9', '0.95',
    '1.0'
]
loadratioList1=['0.5']
lbsNameList = ['ecmp']
alltopoDirlist=['railOnly','dragonfly','fatTree']
topoDirlist=['railOnly','dragonfly']
allWorkloadNamelist=['DCTCP_CDF','RPC_CDF','VL2_CDF']
workloadNamelist=['LLM_INFER_LLAMA','DCTCP_CDF','RPC_CDF','VL2_CDF','FbHdp2015','GoogleRPC2008','AliStorage2019']
def runBigSimTest():
  
    for patternName, patternLoadRatioShift in patternNameMap.items():
        for loadratio in allLoadratioList:
            for lbsName in allLbsNameList:
                fileIdx = experimentalName+"_"+workloadName + "_" +\
                    patternName + "-lr-"+loadratio+"-lb-"+lbsName
                patternFile = vm_patternFiles_path + 'fat-tree-4-8-8-16' + \
                    "-" + "{}-4".format(patternName) + ".txt"

                Line_command = '\
                ./waf --run "scratch/{}\
                --fileIdx={}\
                --outputFileDir={}\
                --inputFileDir={}\
                --topoFileName={}\
                --configFileName={}\
                --simStartTimeInSec={}\
                --qlenMonitorIntervalInNs={}\
                --simEndTimeInSec={}\
                --flowLunchEndTimeInSec={}\
                --lbsName={}\
                --flowletTimoutInUs={}\
                --loadRatioShift={}\
                --loadRatio={}\
                --ccMode={}\
                --screenDisplayInNs={}\
                --enablePfcMonitor={}\
                --enableFctMonitor={}\
                --enableQlenMonitor={}\
                --enableQbbTrace={}\
                --rdmaAppStartPort={}\
                --testPktNum={}\
                --workloadFile={} --patternFile={}\
                --SMTFile={} --PITFile={} --PSTFile={}"\
                '.format(mainFileName, fileIdx, vm_outputFiles_path,
                         vm_inputFiles_path,
                         vm_inputFiles_path + args.topoFileName,
                         vm_inputFiles_path + args.configFileName,
                         args.simStartTimeInSec, args.qlenMonitorIntervalInNs,
                         args.simEndTimeInSec, args.flowLunchEndTimeInSec,
                         lbsName, args.flowletTimoutInUs,
                         patternLoadRatioShift, loadratio,
                         args.ccMode,
                         args.screenDisplayInNs,
                         args.enablePfcMonitor,
                         args.enableFctMonitor,
                         args.enableQlenMonitor,
                         args.enableQbbTrace,
                         args.rdmaAppStartPort,
                         args.testPktNum,
                         workloadFile,patternFile,
                         vm_inputFiles_path + args.smtFileName,vm_inputFiles_path + args.pitFileName,vm_inputFiles_path + args.pstFileName)
                print(Line_command)
                os.system(Line_command)


def runLBSimTest():
    enableFlowCongestTest=True
    # Ring

    for patternName, patternLoadRatioShift in onePatternNameMap.items():
        # 0.7
        for loadratio in loadratioList:
            # conga
            for lbsName in lbsNameList:
                fileIdx = experimentalName+"_"+workloadName + "_" +\
                    patternName + "-lr-"+loadratio+"-lb-"+lbsName
                patternFile = vm_inputFiles_path +"singpare.txt"
                Line_command = '\
                ./waf --run "scratch/{}\
                --fileIdx={}\
                --outputFileDir={}\
                --inputFileDir={}\
                --topoFileName={}\
                --configFileName={}\
                --simStartTimeInSec={}\
                --qlenMonitorIntervalInNs={}\
                --simEndTimeInSec={}\
                --flowLunchEndTimeInSec={}\
                --lbsName={}\
                --flowletTimoutInUs={}\
                --loadRatioShift={} --loadRatio={} \
                --ccMode={}\
                --screenDisplayInNs={}\
                --enablePfcMonitor={}\
                --enableFctMonitor={}\
                --enableQlenMonitor={}\
                --enableQbbTrace={}\
                --rdmaAppStartPort={}\
                --testPktNum={}\
                --workloadFile={} --patternFile={}\
                --SMTFile={} --PITFile={} --PSTFile={}\
                --enableFlowCongestTest={}"\
                '.format(mainFileName, fileIdx, vm_outputFiles_path,
                        vm_inputFiles_path,
                        vm_inputFiles_path + args.topoFileName,
                        
                        vm_inputFiles_path + args.configFileName,
                        args.simStartTimeInSec, args.qlenMonitorIntervalInNs,
                        args.simEndTimeInSec, args.flowLunchEndTimeInSec,
                        lbsName, args.flowletTimoutInUs,
                        patternLoadRatioShift, loadratio,
                        args.ccMode,
                        args.screenDisplayInNs,
                        args.enablePfcMonitor,
                        args.enableFctMonitor,
                        args.enableQlenMonitor,
                        args.enableQbbTrace,
                        args.rdmaAppStartPort,
                        args.testPktNum,
                        workloadFile,patternFile,
                        vm_inputFiles_path + args.smtFileName,vm_inputFiles_path + args.pitFileName,vm_inputFiles_path + args.pstFileName,
                        enableFlowCongestTest)
                print(Line_command)
                os.system(Line_command)
def runTopoSimTest():
    enableFlowCongestTest=True
    # Ring
    for toponame in topoDirlist:
        
        for patternName, patternLoadRatioShift in onePatternNameMap.items():
            # 0.7
            
            for workloadName  in workloadNamelist:
                if workloadName=="LLM_INFER_LLAMA":
                    EnableLLM=True
                    loadratioList=loadratioList1
                else:
                    EnableLLM= args.enableLLMWorKLoad   
                    loadratioList=loadratioListall
                # conga
                lbsName=m_PS2lb[args.PS]
                if lbsName=="e2elaps":
                    ccMode='Laps'
                    pitDir=vm_inputFiles_path + toponame+"/"+ "laps-"+args.pitFileName
                    pstDir=vm_inputFiles_path + toponame+"/"+ "laps-"+args.pstFileName
                    
                else:
                    ccMode=args.ccMode
                    pitDir=vm_inputFiles_path + toponame+"/"+ args.pitFileName
                    pstDir=vm_inputFiles_path + toponame+"/"+ args.pstFileName
                
                for loadratio in loadratioList:
                    workloadFile = vm_workload_path + workloadName + ".txt"
                    fileIdx = experimentalName+"_"+toponame+"_"+workloadName + "_" +\
                        patternName + "-lr-"+loadratio+"-lb-"+lbsName
                    patternFile = vm_inputFiles_path +toponame+"/"+"TFC-"+patternName+".txt"
                  
                    Line_command = '\
                    ./waf --run "scratch/{}\
                    --fileIdx={}\
                    --outputFileDir={}\
                    --inputFileDir={}\
                    --topoFileName={}\
                    --configFileName={}\
                    --simStartTimeInSec={}\
                    --qlenMonitorIntervalInNs={}\
                    --simEndTimeInSec={}\
                    --flowLunchStartTimeInSec={}\
                    --flowLunchEndTimeInSec={}\
                    --lbsName={}\
                    --flowletTimoutInUs={}\
                    --loadRatioShift={} --loadRatio={} \
                    --ccMode={}\
                    --screenDisplayInNs={}\
                    --enablePfcMonitor={}\
                    --enableFctMonitor={}\
                    --enableQlenMonitor={}\
                    --enableQbbTrace={}\
                    --rdmaAppStartPort={}\
                    --testPktNum={}\
                    --workloadFile={} --patternFile={}\
                    --SMTFile={} --PITFile={} --PSTFile={}\
                    --enableFlowCongestTest={}\
                    --enableLLMWorkLoadTest={}"\
                    '.format(mainFileName, fileIdx, vm_outputFiles_path,
                            vm_inputFiles_path,
                            vm_inputFiles_path + toponame+"/"+args.topoFileName,
                            vm_inputFiles_path+
                            vm_inputFiles_path + args.configFileName,
                            args.simStartTimeInSec, args.qlenMonitorIntervalInNs,
                            args.simEndTimeInSec, args.flowLunchStartTimeInSec,args.flowLunchEndTimeInSec,
                            lbsName, args.flowletTimoutInUs,
                            patternLoadRatioShift, loadratio,
                            ccMode,
                            args.screenDisplayInNs,
                            args.enablePfcMonitor,
                            args.enableFctMonitor,
                            args.enableQlenMonitor,
                            args.enableQbbTrace,
                            args.rdmaAppStartPort,
                            args.testPktNum,
                            workloadFile,patternFile,
                            vm_inputFiles_path + toponame+"/"+ args.smtFileName,pitDir,pstDir,
                            enableFlowCongestTest,
                            EnableLLM
                            )
                    print(Line_command)
                    os.system(Line_command)


runTopoSimTest()
