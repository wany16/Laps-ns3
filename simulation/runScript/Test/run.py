import os
import sys
import argparse
import shutil
import os
import sys
import argparse
import shutil
import time

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
            if not os.path.exists(dst_file) or os.path.getmtime(src_file) > os.path.getmtime(dst_file):
                shutil.copy2(src_file, dst_file)
                print(f"File {src_file} is copied to {dst_file}.")

def get_dir_root_host():
    if os.path.exists("/file-in-ctr/"):
        return "/file-in-ctr/"
    elif os.path.exists("/file-in-cntr/"):
        return "/file-in-cntr/"
    else:
        print("No such directory: /file-in-ctr/ or /file-in-cntr/")
        return None

def update_module_files(dir_root_host, dir_root_ns3, sydDirList):
    for sydDir in sydDirList:
        override_files(dir_root_host + sydDir, dir_root_ns3 + sydDir)

def get_experiment_details():
    current_file_path = os.path.abspath(__file__)
    current_dir_path = os.path.dirname(current_file_path)
    current_dir_name = os.path.basename(current_dir_path)
    experimentalName = current_dir_name
    mainFileName = "main"
    return experimentalName, mainFileName

def update_main_file(dir_ns3_scratch, dir_host_runScript, mainFileName):
    if mainFileName is None:
        print("Failed to get experiment details.")
        sys.exit(1)
    target_mainFile_path = os.path.join(dir_ns3_scratch, mainFileName + ".cc")
    source_mainFile_path = os.path.join(dir_host_runScript, mainFileName + ".cc")
    if os.path.exists(target_mainFile_path):
        os.remove(target_mainFile_path)
    shutil.copy2(source_mainFile_path, target_mainFile_path)

def create_directory(dstDir):
    os.makedirs(dstDir, exist_ok=True)

experimentalName, mainFileName = get_experiment_details()
dir_host_root = get_dir_root_host()
dir_host_workloads = dir_host_root + "simulation/" + "workloads/"
create_directory(dir_host_workloads)
dir_host_topologies = dir_host_root + "simulation/" + "topologies/"
create_directory(dir_host_topologies)
dir_host_results = dir_host_root + "simulation/" + "results/" + experimentalName + "/"
create_directory(dir_host_results)
dir_host_patterns = dir_host_root + "simulation/" + "patterns/"
create_directory(dir_host_patterns)
dir_host_configures = dir_host_root + "simulation/" + "configures/"
create_directory(dir_host_configures)
dir_host_runScript = dir_host_root + "simulation/" + "runScript/" + experimentalName + "/"
create_directory(dir_host_runScript)
dir_ns3_root = "/app/ns3-detnet-rdma-main/ns-3.33/"
dir_ns3_scratch = dir_ns3_root + "scratch/"


# 调用函数更新模块文件
sydDirList = ['src']
update_module_files(dir_host_root, dir_ns3_root, sydDirList)
# 调用函数获取实验详情
# update the main.cc
update_main_file(dir_ns3_scratch, dir_host_runScript, mainFileName)



#

parser = argparse.ArgumentParser(description="请输入以下参数")
parser.add_argument("--configFileName", default="CONFIG_DCQCN-test-00001.txt", help="defaultFileName, by default CONFIG.txt")
parser.add_argument("--topoFileName", default="TOPO_S5_H4_L10.txt",  help="defaultFileName, by default fat_tree_topology.txt")
parser.add_argument("--simStartTimeInSec", default="0", help="simulation start time")
parser.add_argument("--simEndTimeInSec", default="0.005",  help="simulation end time")
parser.add_argument("--flowLunchEndTimeInSec", default="0.001", help="flow end time")
parser.add_argument("--qlenMonitorIntervalInNs", default="100000", help="Qlen Monitor period In Ns")
parser.add_argument("--lbsName", default="e2elaps", help="Load balancing algorithm")
parser.add_argument("--flowletTimoutInUs", default="50", help="The time out of the flowlet in microsecond.")
parser.add_argument("--loadRatioShift", default="1.0",  help="loadfactorAdjustFacror:Ring ->1,all2all->1/(n-1),Reduce->1/(K-1),n is host num,k is group num.")
parser.add_argument("--loadratio", default="1",  help="The ratio of the load")
parser.add_argument("--ccMode", default="Laps",  help="congestion control algorithm")
parser.add_argument("--screenDisplayInNs", default="10000000",  help="screen display interval in Ns")
parser.add_argument("--enablePfcMonitor", default="true",  help="trace Pfc packets or not ")
parser.add_argument("--enableFctMonitor", default="true",  help="trace Fct or not")
parser.add_argument("--enableQlenMonitor", default="false",  help="trace queue length or not")
parser.add_argument("--rdmaAppStartPort", default="6666",  help="minimal port for rdma client")
parser.add_argument("--enableQbbTrace", default="true",  help="trace the packet event on node's all Qbb netdevices")
parser.add_argument("--testPktNum", default="4000",  help="The number of packets to test")
parser.add_argument("--workloadFile", default="DCTCP.txt",  help="The workload file")
parser.add_argument("--patternFile", default="spine-leaf-2-4-16-Ring.txt",  help="The pattern file")
parser.add_argument("--pstFile", default="PST_S5_H4_L10.txt",  help="The pstFile file")
parser.add_argument("--pitFile", default="PIT_S5_H4_L10.txt",  help="The pitFile file")
parser.add_argument("--smtFile", default="SMT_S5_H4_L10.txt",  help="The smtFile file")
args = parser.parse_args()


os.chdir(dir_ns3_root)
print("Working Directoty: ", os.getcwd())

Line_command = '\
    ./waf --run "scratch/{}\
    --fileIdx={}\
    --outputFileDir={}\
    --topoFileName={}\
    --simStartTimeInSec={}\
    --qlenMonitorIntervalInNs={}\
    --simEndTimeInSec={}\
    --lbsName={}\
    --flowletTimoutInUs={}\
    --loadRatioShift={}\
    --ccMode={}\
    --screenDisplayInNs={}\
    --enablePfcMonitor={}\
    --enableFctMonitor={}\
    --enableQlenMonitor={}\
    --enableQbbTrace={}\
    --rdmaAppStartPort={}\
    --testPktNum={}\
    --loadRatio={} --workloadFile={} --patternFile={}\
    --PITFile={} --PSTFile={} --SMTFile={}"\
'.format(
    mainFileName,
    experimentalName,
    dir_host_results,
    dir_host_topologies + args.topoFileName,
    args.simStartTimeInSec,
    args.qlenMonitorIntervalInNs,
    args.simEndTimeInSec,
    args.lbsName,
    args.flowletTimoutInUs,
    args.loadRatioShift,
    args.ccMode,
    args.screenDisplayInNs,
    args.enablePfcMonitor,
    args.enableFctMonitor,
    args.enableQlenMonitor,
    args.enableQbbTrace,
    args.rdmaAppStartPort,
    args.testPktNum,
    args.loadratio,
    dir_host_workloads + args.workloadFile,
    dir_host_patterns + args.patternFile,
    dir_host_topologies + args.pitFile,
    dir_host_topologies + args.pstFile,
    dir_host_topologies + args.smtFile)
print(Line_command)
os.system(Line_command)
