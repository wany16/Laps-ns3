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
    LogComponentEnable("RdmaHw", LOG_LEVEL_INFO);
    //  LogComponentEnable("SwitchNode", LOG_LEVEL_INFO);
    LogComponentEnable("ConWeaveRouting", LOG_LEVEL_INFO);
    //  LogComponentEnable("RdmaSmartFlowRouting", LOG_LEVEL_INFO);
    LogComponentEnable("userdefinedfunction", LOG_LEVEL_INFO);
    // LogComponentEnable("CongestionControlSimulator", LOG_LEVEL_INFO);
    // LogComponentEnable("QbbNetDevice", LOG_LEVEL_INFO);
    // LogComponentEnable ("BEgressQueue", LOG_LEVEL_INFO);

    global_variable_t varMap;
    varMap.configFileName = "/file-in-ctr/inputFiles/C00002/CONFIG_DCQCN.txt";
    std::cout << "*******************************Parse the Default Configures*****************************************" << std::endl;
    parse_default_configures(&varMap);
    std::cout << "*******************************Parse the Input Parameters*****************************************" << std::endl;
    CommandLine cmd;
    cmd.AddValue("configFileName", "configFileName", varMap.configFileName);
    cmd.AddValue("topoFileName", "topoFileName", varMap.topoFileName);
    cmd.AddValue("inputFileDir", "input File Directory", varMap.inputFileDir);
    cmd.AddValue("outputFileDir", "output File Directory", varMap.outputFileDir);
    cmd.AddValue("fileIdx", "fileIdx", varMap.fileIdx);
    cmd.AddValue("simStartTimeInSec", "simStartTimeInSec", varMap.simStartTimeInSec);
    cmd.AddValue("simEndTimeInSec", "simEndTimeInSec", varMap.simEndTimeInSec);
    cmd.AddValue("flowLunchStartTimeInSec", "flowLunchStartTimeInSec", varMap.flowLunchStartTimeInSec);
    cmd.AddValue("flowLunchEndTimeInSec", "flowLunchEndTimeInSec", varMap.flowLunchEndTimeInSec);
    cmd.AddValue("qlenMonitorIntervalInNs", "qlenMonitorIntervalInNs", varMap.qlenMonitorIntervalInNs);
    cmd.AddValue("lbsName", "Name of the load balancing solution.", varMap.lbsName);
    cmd.AddValue("flowletTimoutInUs", "The time out of the flowlet in microsecond.", varMap.flowletTimoutInUs);
    cmd.AddValue("loadRatio", "The load ratio of the server workload.", varMap.loadRatio);
    cmd.AddValue("loadRatioShift", "The load AdjustFacror of the server workload.", varMap.loadRatioShift);
    cmd.AddValue("workloadFile", "The load source of the workload.", varMap.workLoadFileName);
    cmd.AddValue("patternFile", "The server run pattern.", varMap.patternFile);
    cmd.AddValue("SMTFile", "init The LAPS LB SMT map.", varMap.smtFile);
    cmd.AddValue("PITFile", "init The LAPS LB PIT map.", varMap.pitFile);
    cmd.AddValue("PSTFile", "init The LAPS LB PST map.", varMap.pstFile);
    cmd.AddValue("ccMode", "congestion control algorithm.", varMap.ccMode);
    cmd.AddValue("screenDisplayInNs", "creen display interval in Ns.", varMap.screenDisplayInNs);
    cmd.AddValue("enablePfcMonitor", "trace Pfc packets or not.", varMap.enablePfcMonitor);
    cmd.AddValue("enableFctMonitor", "trace Fct or not", varMap.enableFctMonitor);
    cmd.AddValue("enableQlenMonitor", "trace queue length or not.", varMap.enableQlenMonitor);
    cmd.AddValue("rdmaAppStartPort", "minimal port for rdma client.", varMap.appStartPort);
    cmd.AddValue("enableQbbTrace", "trace the packet event on node's all Qbb netdevices.", varMap.enableQbbTrace);
    cmd.AddValue("testPktNum", "The number of packets to test.", varMap.testPktNum);
    cmd.AddValue("enableFlowCongestTest", "creat network congestion Test", varMap.enableFlowCongestTest);
    cmd.Parse(argc, argv);
    update_EST(varMap.paraMap, "loadRatio", varMap.loadRatio);

    std::cout << "*******************************Load the Default Configures*****************************************" << std::endl;
    load_default_configures(&varMap);
    std::cout << "-------------------------------Create The Topology----------------------------------------" << std::endl;
    create_topology_rdma(&varMap);
    std::cout << "-------------------------------Assign The Addresses----------------------------------------" << std::endl;
    // assign_addresses(varMap.allNodes, varMap.addr2node);
    assign_node_addresses(&varMap);
    std::cout << "-------------------------------Calculate The Paths----------------------------------------" << std::endl;
    calculate_paths_for_servers(&varMap);
    std::cout << "-------------------------------Install The Routing Table----------------------------------------" << std::endl;
    install_routing_entries_without_Pathtable(&varMap);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    // print_node_routing_tables(&varMap, 5);
    // print_node_routing_tables(&varMap, 19);
    std::cout << "-------------------------------Configure The Switch----------------------------------------" << std::endl;
    config_switch(&varMap);
    std::cout << "-------------------------------Install The Application----------------------------------------" << std::endl;
    // install_rdma_client_on_node(&varMap, 20, 24);
    // install_rdma_client_on_node(&varMap, 20, 21, 1, 2000000000, varMap.appStartPort);
    // install_rdma_client_on_node(&varMap, 20, 30, 1, 2000000000, varMap.appStartPort + 1);
    /*for (size_t i = 0; i < 200; i += 4)
    {
        install_rdma_client_on_node(&varMap, 5, 7, 1, 20000000, varMap.appStartPort);
        install_rdma_client_on_node(&varMap, 5, 8, 1, 20000000, varMap.appStartPort + i + 1);
        install_rdma_client_on_node(&varMap, 6, 8, 1, 20000000, varMap.appStartPort + i + 2);
        install_rdma_client_on_node(&varMap, 6, 7, 1, 20000000, varMap.appStartPort + i + 3);
    }*/

    /* for (size_t i = 0; i < 200; i += 1)
     {
         install_rdma_client_on_node(&varMap, 5, 7, 1, 20000, varMap.appStartPort + i);
     }*/
    // install_rdma_client_on_node(&varMap, 5, 7, 1, 3000, varMap.appStartPort);

    install_rdma_client_on_node(&varMap, 5, 8, 1, 1000, varMap.appStartPort + 1);
    // install_rdma_client_on_node(&varMap, 6, 8, 1, 20000000000, varMap.appStartPort + 2);
    // install_rdma_client_on_node(&varMap, 6, 7, 1, 20000000000, varMap.appStartPort + 3);
    //  node_install_rdma_application(&varMap);
    std::cout << "-------------------------------Monitor The queue Len----------------------------------------" << std::endl;
    monitor_special_port_qlen(&varMap, 0, 1, 0);
    std::cout << "-------------------------------Monitor The qBB Device----------------------------------------" << std::endl;
    set_QBB_trace(&varMap);
    std::cout << "-------------------------------Start the Simulation----------------------------------------" << std::endl;
    Simulator::Stop(Seconds(varMap.simEndTimeInSec));
    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    save_egress_ports_loadinfo(&varMap);
    sim_finish(&varMap);
    Simulator::Destroy();
    std::cout << "-------------------------------Finish The Simulation----------------------------------------" << std::endl;
    return 0;
}
