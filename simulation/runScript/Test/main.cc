/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <time.h>
#include "ns3/core-module.h"
#include "ns3/qbb-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/error-model.h"
#include <ns3/rdma.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-driver.h>
#include <ns3/switch-node.h>
#include <ns3/sim-setting.h>
#include <ns3/userdefinedfunction.h>
#include "ns3/trace-helper.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("CongestionControlSimulator");

int main(int argc, char *argv[])
{
    // LogComponentEnable("userdefinedfunction", LOG_LEVEL_FUNCTION);
    // LogComponentEnable("CongestionControlSimulator", LOG_LEVEL_INFO);
    // LogComponentEnable("QbbNetDevice", LOG_LEVEL_INFO);
    // LogComponentEnable ("BEgressQueue", LOG_LEVEL_INFO);
    // LogComponentEnable("RdmaQueuePair", LOG_LEVEL_INFO);
    // LogComponentEnable("SwitchNode", LOG_LEVEL_INFO);
    // LogComponentEnable("RdmaHw", LOG_LEVEL_INFO);
    // LogComponentEnable("SwitchNode", LOG_LEVEL_INFO);
    // LogComponentEnable("RdmaSmartFlowRouting", LOG_LEVEL_INFO);
    // LogComponentEnable("userdefinedfunction", LOG_LEVEL_INFO);
    LogComponentEnable("CongestionControlSimulator", LOG_LEVEL_INFO);

    std::cout << "*******************************Parse the Default Parameters*****************************************" << std::endl;
    global_variable_t varMap;
    varMap.configFileName = "/file-in-ctr/simulation/configures/CONFIG_DCQCN-test-00001.txt";  
    parse_default_configures(&varMap);

    std::cout << "*******************************Parse the Input Parameters*****************************************" << std::endl;
    CommandLine cmd;

    cmd.AddValue("topoFileName", "topoFileName", varMap.topoFileName);
    cmd.AddValue("PITFile", "init The LAPS LB PIT map.", varMap.pitFile);
    cmd.AddValue("PSTFile", "init The LAPS LB PST map.", varMap.pstFile);
    cmd.AddValue("SMTFile", "init The LAPS LB SMT map", varMap.smtFile);
    cmd.AddValue("inputFileDir", "input File Directory", varMap.inputFileDir);
    cmd.AddValue("outputFileDir", "output File Directory", varMap.outputFileDir);
    cmd.AddValue("fileIdx", "fileIdx", varMap.fileIdx);
    cmd.AddValue("simStartTimeInSec", "simStartTimeInSec", varMap.simStartTimeInSec);
    cmd.AddValue("simEndTimeInSec", "simEndTimeInSec", varMap.simEndTimeInSec);
    cmd.AddValue("qlenMonitorIntervalInNs", "qlenMonitorIntervalInNs", varMap.qlenMonitorIntervalInNs);
    cmd.AddValue("lbsName", "Name of the load balancing solution.", varMap.lbsName);
    cmd.AddValue("flowletTimoutInUs", "The time out of the flowlet in microsecond.", varMap.flowletTimoutInUs);
    cmd.AddValue("loadRatio", "The load ratio of the server workload.", varMap.loadRatio);
    cmd.AddValue("loadRatioShift", "The load AdjustFacror of the server workload.", varMap.loadRatioShift);
    cmd.AddValue("workloadFile", "The load source of the workload.", varMap.workLoadFileName);
    cmd.AddValue("patternFile", "The server run pattern.", varMap.patternFile);
    cmd.AddValue("ccMode", "congestion control algorithm.", varMap.ccMode);
    cmd.AddValue("screenDisplayInNs", "creen display interval in Ns.", varMap.screenDisplayInNs);
    cmd.AddValue("enablePfcMonitor", "trace Pfc packets or not.", varMap.enablePfcMonitor);
    cmd.AddValue("enableFctMonitor", "trace Fct or not", varMap.enableFctMonitor);
    cmd.AddValue("enableQlenMonitor", "trace queue length or not.", varMap.enableQlenMonitor);
    cmd.AddValue("rdmaAppStartPort", "minimal port for rdma client.", varMap.appStartPort);
    cmd.AddValue("enableQbbTrace", "trace the packet event on node's all Qbb netdevices.", varMap.enableQbbTrace);
    cmd.AddValue("testPktNum", "The number of packets to test.", varMap.testPktNum);

    cmd.Parse(argc, argv);

    std::cout << "*******************************Load the Default Parameters*****************************************" << std::endl;
    load_default_configures(&varMap);
    std::cout << "-------------------------------Create The Topology----------------------------------------" << std::endl;
    create_topology_rdma(&varMap);
    std::cout << "-------------------------------Assign The Addresses----------------------------------------" << std::endl;
    assign_addresses(varMap.allNodes, varMap.addr2node);
    // std::cout << "-------------------------------Calculate The Paths----------------------------------------" << std::endl;
    calculate_paths_for_servers(&varMap);
    std::cout << "-------------------------------Install The Routing Table----------------------------------------" << std::endl;
    install_routing_entries(&varMap);
    // Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    std::cout << "-------------------------------Configure The Switch----------------------------------------" << std::endl;
    config_switch(&varMap);
    std::cout << "-------------------------------Install The Application----------------------------------------" << std::endl;
    install_rdma_client_on_node(&varMap, 5, 8, 1, varMap.testPktNum, varMap.appStartPort);
    install_rdma_client_on_node(&varMap, 6, 8, 1, varMap.testPktNum, varMap.appStartPort+1);
    // install_rdma_client_on_node(&varMap, 7, 8, 1, varMap.testPktNum, varMap.appStartPort+2);

    // install_rdma_client_on_node(&varMap, 5, 7, 1, varMap.testPktNum, varMap.appStartPort);

    // install_rdma_client_on_node(&varMap, 6, 8, varMap.appStartPort+1);
    // install_kv_cache_applications(&varMap);
    /*
    Ptr<Node> srcnode = varMap.svNodes.Get(0);
    Ptr<Node> dstnode = varMap.svNodes.Get(1);
    // 获取节点的第一个接口的IPv4地址
    Ptr<Ipv4> srcipv4_1 = srcnode->GetObject<Ipv4>();
    // uint32_t srcinterfaceIndex_1 = interfaces.GetInterfaceIndex(srcnode, 0);
    Ipv4Address srcipAddr1 = srcipv4_1->GetAddress(1, 0).GetLocal();
    // 获取节点2的第一个接口的IPv4地址
    Ptr<Ipv4> dstipv4_1 = dstnode->GetObject<Ipv4>();
    // uint32_t dstinterfaceIndex_1 = interfaces.GetInterfaceIndex(dstnode, 0);
    Ipv4Address dstipAddr1 = dstipv4_1->GetAddress(1, 0).GetLocal();

    std::cout << "-------------------------------Monitor The queue Len----------------------------------------" << std::endl;
    monitor_special_port_qlen(&varMap, 0, 1, 0);
    std::cout << "-------------------------------Monitor The qBB Device----------------------------------------" << std::endl;
    set_QBB_trace(&varMap);
    /*std::cout << "-------------------------------Monitor Device for pcap trace----------------------------------------" << std::endl;
    std::string prefix = "rdma_pcap_trace";
    NetDeviceContainer nodeDevices = get_all_netdevices_of_a_node(srcnode);

    for (uint32_t i = 0; i < nodeDevices.GetN(); ++i)
    {
        std::stringstream ss;
        ss << varMap.outputFileDir << "/" << prefix << "_" << i;
        pcapHelper.EnablePcap(ss.str(), nodeDevices.Get(i));
    }*/
    std::cout << "-------------------------------Start the Simulation----------------------------------------" << std::endl;
    Simulator::Stop(Seconds(varMap.simEndTimeInSec));
    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    // switchportinfoPrint(&varMap, 3);
    // monitor rps lb node
    // std::cout << map_to_string<uint32_t, uint32_t>(SwitchNode::m_rpsPortInf[3]) << std::endl;

    /* //monitor ecmp node
    for (uint32_t i = 1; i < varMap.swNodes.Get(3)->GetNDevices(); i++)
    {
        if (SwitchNode::m_ecmpPortInf[3].find(i) != SwitchNode::m_ecmpPortInf[3].end())
        {
        std::cout << "SwitchNode::m_ecmpPortInf[3] size: " << SwitchNode::m_ecmpPortInf[3].size() << ",port id is " << i << " " << map_to_string<uint32_t, uint32_t>(SwitchNode::m_ecmpPortInf[3][i]) << std::endl;

        }

    }
    */
    //  std::cout << map_to_string<uint32_t, uint32_t>(SwitchNode::m_rpsPortInf[3]) << std::endl;
    sim_finish(&varMap);
    Simulator::Destroy();
    std::cout << "-------------------------------Finish The Simulation----------------------------------------" << std::endl;
    return 0;
}
