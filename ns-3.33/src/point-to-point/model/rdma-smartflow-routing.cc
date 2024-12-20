// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2008 University of Washington
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "rdma-smartflow-routing.h"
#include "assert.h"
#include <ns3/seq-ts-header.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include "ns3/ppp-header.h"
#include "ns3/flow-id-tag.h"
namespace ns3
{

    // 在类外部初始化静态成员变量
    std::vector<probeInfoEntry> RdmaSmartFlowRouting::m_prbInfoTable(0);
    std::map<std::string, reorder_entry_t> RdmaSmartFlowRouting::m_reorderTable;

    NS_LOG_COMPONENT_DEFINE("RdmaSmartFlowRouting");

    NS_OBJECT_ENSURE_REGISTERED(RdmaSmartFlowRouting);

    RdmaSmartFlowRouting::RdmaSmartFlowRouting() : // m_ipv4(0),
                                                   m_alpha(0.2),
                                                   // Quantizing bits
                                                   m_Q(3),
                                                   // m_C(DataRate("10Gbps")),
                                                   m_agingTime(MilliSeconds(10)),
                                                   m_tdre(MicroSeconds(100)),
                                                   m_probeTimeInterval(PROBE_DEFAULT_INTERVAL_IN_NANOSECOND),
                                                   m_pathExpiredTimeThld(PROBE_PATH_EXPIRED_TIME_IN_NANOSECOND),
                                                   m_pathSelStrategy(PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY),
                                                   m_piggybackStrategy(PIGGY_BACK_SMALL_SENT_TIME_FIRST_STRATEGY),
                                                   m_pathSelNum(DEFAULT_PATH_SELECTION_NUM),
                                                   m_piggyLatencyCnt(PIGGY_BACK_DEFAULT_PATH_NUMBER),
                                                   m_enableFeedbackAllPathInfo(false),
                                                   m_enabledAllPacketFeedbackInfo(true)

    {
        NS_LOG_FUNCTION(this);
        m_pathSelTbl.clear();
        m_nexthopSelTbl.clear();
        m_vmVtepMapTbl.clear();
    }
    RdmaSmartFlowRouting::~RdmaSmartFlowRouting()
    {
        NS_LOG_FUNCTION(this);
    }

    bool cmp_pitEntry_in_increase_order_of_latency(const PathData *lhs, const PathData *rhs)
    {
        return lhs->latency < rhs->latency; // 升序排列
    }

    bool cmp_pitEntry_in_decrease_order_of_priority(const PathData *lhs, const PathData *rhs)
    {
        return lhs->priority > rhs->priority; // 降序排列
    }

    bool cmp_pitEntry_in_increase_order_of_Generation_time(const PathData *lhs, const PathData *rhs)
    {
        return lhs->tsGeneration < rhs->tsGeneration; // 升序排列
    }

    bool cmp_pitEntry_in_increase_order_of_congestion_Degree(const PathData *lhs, const PathData *rhs)
    {
        return lhs->pathDre < rhs->pathDre; // 升序排列
    }

    bool cmp_pitEntry_in_increase_order_of_Sent_time(const PathData *lhs, const PathData *rhs)
    {
        return lhs->tsLatencyLastSend < rhs->tsLatencyLastSend; // 升序排列
    }

    void RdmaSmartFlowRouting::SetSwitchInfo(bool isToR, uint32_t switch_id)
    {
        m_isToR = isToR;
        m_switch_id = switch_id;
    }

    TypeId RdmaSmartFlowRouting::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::RdmaSmartFlowRouting")
                                .SetParent<Object>()
                                .SetGroupName("Internet")
                                .AddConstructor<RdmaSmartFlowRouting>()
                                .AddAttribute("probeTimeInterval", " Probe can be send only exceeds this time interval",
                                              UintegerValue(PROBE_DEFAULT_INTERVAL_IN_NANOSECOND),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_probeTimeInterval),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("pathExpiredTimeThld", " Probe can be send only exceeds this time interval",
                                              UintegerValue(PROBE_PATH_EXPIRED_TIME_IN_NANOSECOND),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_pathExpiredTimeThld),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("pathSelStrategy", " how to select the path based on the latency",
                                              UintegerValue(PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_pathSelStrategy),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("reorderFlag", " reorderFlag or not",
                                              UintegerValue(0),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_reorderFlag),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("piggybackStrategy", " how to take away the piggyback data",
                                              UintegerValue(PIGGY_BACK_SMALL_SENT_TIME_FIRST_STRATEGY),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_piggybackStrategy),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("probePathStrategy", " how to select the path to probe",
                                              UintegerValue(PROBE_SMALL_LATENCY_FIRST_STRATEGY),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_probeStrategy),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("flowletTimoutInNs", " flowlet Timout In Ns",
                                              UintegerValue(FLOWLET_DEFAULT_TIMEOUT_IN_NANOSECOND),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_flowletTimoutInNs),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("pathSelNum", " path selection number",
                                              UintegerValue(DEFAULT_PATH_SELECTION_NUM),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_pathSelNum),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("piggyLatencyCnt", " piggy back Latency count",
                                              UintegerValue(PIGGY_BACK_DEFAULT_PATH_NUMBER),
                                              MakeUintegerAccessor(&RdmaSmartFlowRouting::m_piggyLatencyCnt),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("enableFeedbackAllPathInfo", " piggy Feed back all path info ",
                                              BooleanValue(false),
                                              MakeBooleanAccessor(&RdmaSmartFlowRouting::m_enableFeedbackAllPathInfo),
                                              MakeBooleanChecker())
                                .AddAttribute("enabledAllPacketFeedbackInfo",
                                              "all kind of packet feed back path info ",
                                              BooleanValue(true),
                                              MakeBooleanAccessor(&RdmaSmartFlowRouting::m_enabledAllPacketFeedbackInfo),
                                              MakeBooleanChecker());

        return tid;
    }
    std::string RdmaSmartFlowRouting::ipv4Address2string(Ipv4Address addr)
    {
        std::ostringstream os;
        addr.Print(os);
        std::string addrStr = os.str();
        return addrStr;
    }
    hostIp2SMT_entry_t *RdmaSmartFlowRouting::lookup_SMT(const Ipv4Address &serverAddr)
    {
        // NS_LOG_INFO ("############ Fucntion: lookup_VMT() ############");
        // NS_LOG_INFO("VMT_KEY: svAddr=" << serverAddr);
        // hostIp2SMT_entry_t vmtEntry;

        std::map<Ipv4Address, hostIp2SMT_entry_t>::iterator it;
        it = m_vmVtepMapTbl.find(serverAddr);
        if (it == m_vmVtepMapTbl.end())
        {
            std::cout << "Error in lookup_SMT() since Cannot match any entry in SMT for the Key: (";
            std::cout << ipv4Address2string(serverAddr);
            std::cout << std::endl;
            return 0;
        }
        else
        {
            return &(it->second);
        }
    }
    std::string RdmaSmartFlowRouting::construct_target_string_strlen(uint32_t strLen, std::string c)
    {
        std::string result;
        for (uint32_t i = 0; i < strLen; ++i)
        {
            result = result + c;
        }
        return result;
    }

    uint32_t RdmaSmartFlowRouting::print_PIT()
    {
        uint32_t pitSize = m_nexthopSelTbl.size();
        uint32_t nodeID = m_switch_id;
        std::cout << "Node " << nodeID << " has a PIT of " << pitSize << " entries" << std::endl;
        std::cout << "Index" << construct_target_string_strlen(2, " ");
        std::cout << "Pid" << construct_target_string_strlen(3, " ");
        std::cout << "Priority" << construct_target_string_strlen(2, " ");
        std::cout << "Latency" << construct_target_string_strlen(3, " ");
        std::cout << "GenTime" << construct_target_string_strlen(3, " ");
        std::cout << "PbnTime" << construct_target_string_strlen(3, " ");
        std::cout << "SntTime" << construct_target_string_strlen(3, " ");
        std::cout << "NodeID" << construct_target_string_strlen(20, " ");
        std::cout << "PortID" << construct_target_string_strlen(1, " ");
        std::cout << std::endl;
        uint32_t pathIdx = 0;
        std::map<uint32_t, PathData>::iterator it;
        for (it = m_nexthopSelTbl.begin(); it != m_nexthopSelTbl.end(); it++)
        {
            std::cout << pathIdx << construct_target_string_strlen(5 + 2 - change2string(pathIdx).size(), " ");
            PathData *pstEntry = &(it->second);
            uint32_t pid = pstEntry->pid;
            std::cout << pid << construct_target_string_strlen(3 + 3 - change2string(pid).size(), " ");
            uint32_t priority = pstEntry->priority;
            std::cout << priority << construct_target_string_strlen(8 + 2 - change2string(priority).size(), " ");
            uint32_t latency = pstEntry->latency;
            std::cout << latency << construct_target_string_strlen(7 + 3 - change2string(latency).size(), " ");
            uint32_t GenTime = pstEntry->tsGeneration.GetNanoSeconds();
            std::cout << GenTime << construct_target_string_strlen(7 + 3 - change2string(GenTime).size(), " ");
            uint32_t PbnTime = pstEntry->tsProbeLastSend.GetNanoSeconds();
            std::cout << PbnTime << construct_target_string_strlen(7 + 3 - change2string(PbnTime).size(), " ");
            uint32_t SntTime = pstEntry->tsLatencyLastSend.GetNanoSeconds();
            std::cout << SntTime << construct_target_string_strlen(7 + 3 - change2string(SntTime).size(), " ");
            std::string nodeIdsStr = vectorTostring<uint32_t>(pstEntry->nodeIdSequence);
            std::cout << nodeIdsStr << construct_target_string_strlen(20 + 6 - change2string(nodeIdsStr).size(), " ");
            std::string portsStr = vectorTostring<uint32_t>(pstEntry->portSequence);
            std::cout << portsStr << construct_target_string_strlen(1, " ");
            std::cout << std::endl;
            pathIdx = pathIdx + 1;
        }
        return pathIdx;
    }

    /*uint32_t RdmaSmartFlowRouting::print_PST()
    {
        uint32_t nodeID = get_node_id();
        uint32_t pstSize = m_pathSelTbl.size();
        std::cout << "Node: **" << nodeID << "** has PST in smartFlow with **" << pstSize << "** entries" << std::endl;
        std::cout << "Index" << construct_target_string_strlen(5, " ");
        std::cout << "(SrcTorAddr, DstToRAddr)" << construct_target_string_strlen(5, " ");
        std::cout << "PathNum" << construct_target_string_strlen(5, " ");
        std::cout << "LastIdx" << construct_target_string_strlen(2, " ");
        std::cout << "highestIdx" << construct_target_string_strlen(2, " ");
        ;
        std::cout << "PathIDs" << construct_target_string_strlen(5, " ");
        ;
        std::cout << std::endl;
        uint32_t entryCnt = 0;
        std::map<HostId2PathSeleKey, pstEntryData>::const_iterator it;
        for (it = m_pathSelTbl.begin(); it != m_pathSelTbl.end(); it++)
        {
            std::cout << entryCnt << construct_target_string_strlen(5 + 5 - change2string(entryCnt).size(), " ");
            std::string srcTorId = std::string(it->first.selfHostId);
            std::string dstHostId = std::string(it->first.dstHostId);
            std::string pathKeyStr = "(" + srcTorId + ", " + dstHostId+ ")";
            std::cout << pathKeyStr << construct_target_string_strlen(5 + 24 - change2string(pathKeyStr).size(), " ");
            uint32_t curPathNum = it->second.pathNum;
            std::cout << curPathNum << construct_target_string_strlen(5 + 7 - change2string(curPathNum).size(), " ");
            uint32_t lastSelectedPathIdx = it->second.lastSelectedPathIdx;
            std::cout << lastSelectedPathIdx << construct_target_string_strlen(7 + 2 - change2string(lastSelectedPathIdx).size(), " ");
            uint32_t highestPriorityPathIdx = it->second.highestPriorityPathIdx;
            std::cout << highestPriorityPathIdx << construct_target_string_strlen(10 + 2 - change2string(highestPriorityPathIdx).size(), " ");
            std::string pathIdsStr = vectorTostring<uint32_t>(it->second.paths);
            std::cout << pathIdsStr << construct_target_string_strlen(1, " ");
            std::cout << std::endl;
            entryCnt = entryCnt + 1;
        }
        return entryCnt;
    }*/

    uint32_t RdmaSmartFlowRouting::print_SMT()
    {
        uint32_t nodeID = m_switch_id;
        uint32_t vmtSize = m_vmVtepMapTbl.size();
        std::cout << "Node: **" << nodeID << "** has SMT in smartFlow with **" << vmtSize << "** entries" << std::endl;
        std::cout << "Index" << construct_target_string_strlen(5, " ");
        std::cout << "Server Address" << construct_target_string_strlen(6, " ");
        std::cout << "ToR Address" << construct_target_string_strlen(1, " ");
        std::cout << std::endl;
        uint32_t entryCnt = 0;
        std::map<Ipv4Address, hostIp2SMT_entry_t>::const_iterator it;
        for (it = m_vmVtepMapTbl.begin(); it != m_vmVtepMapTbl.end(); it++)
        {
            std::cout << entryCnt << construct_target_string_strlen(5 + 5 - change2string(entryCnt).size(), " ");
            std::cout << ipv4Address2string(it->first) << construct_target_string_strlen(6 + 14 - ipv4Address2string(it->first).size(), " ");
            std::cout << it->second.torId << construct_target_string_strlen(1, " ");
            std::cout << it->second.hostId << construct_target_string_strlen(1, " ");
            std::cout << std::endl;
            entryCnt = entryCnt + 1;
        }
        return entryCnt;
    }

    /** CALLBACK: callback functions  */
    void RdmaSmartFlowRouting::DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev, uint32_t qIndex)
    {
        m_switchSendCallback(p, ch, outDev, qIndex);
    }
    void RdmaSmartFlowRouting::DoSwitchSendToDev(Ptr<Packet> p, CustomHeader &ch)
    {
        m_switchSendToDevCallback(p, ch);
    }

    void RdmaSmartFlowRouting::SetSwitchSendCallback(SwitchSendCallback switchSendCallback)
    {
        m_switchSendCallback = switchSendCallback;
    }

    void RdmaSmartFlowRouting::SetSwitchSendToDevCallback(SwitchSendToDevCallback switchSendToDevCallback)
    {
        m_switchSendToDevCallback = switchSendToDevCallback;
    }

    bool RdmaSmartFlowRouting::output_packet_by_path_tag(Ptr<Packet> packet, CustomHeader &ch, uint32_t pg)
    {
        NS_LOG_INFO("############ Fucntion: output_packet_by_path_tag() ############");
        // std::cout << "Enter output_packet_by_path_tag()"<<std::endl;

        Ipv4SmartFlowPathTag pathTag;
        packet->PeekPacketTag(pathTag);
        // uint32_t maxQLen = pathTag.GetDre();
        uint32_t selectedPortId = get_egress_port_id_by_path_tag(pathTag);

        // if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
        //     uint32_t localQLen = CalculateQueueLength (selectedPortId);
        //     if (localQLen > maxQLen)  {
        //         // std::cout << "InterSwitch update congestion degree: " << "    ";
        //         // std::cout << "Node: " << get_node_id() << "    ";
        //         // std::cout << "Time: " << Simulator::Now().GetNanoSeconds() << "    ";
        //         // std::cout << "Pid: " << pathTag.get_path_id() << "    ";
        //         // std::cout << "QLen: " << maxQLen << " --> " << localQLen << "    ";
        //         // std::cout << "\n";
        //         pathTag.SetDre(localQLen);
        //     }

        // }
        update_path_tag(packet, pathTag);

        // output_packet_by_port_id(packet,ch , ucb, selectedPortId, dstServerAddr);
        DoSwitchSend(packet, ch, selectedPortId, pg);
        // std::cout << "Leave output_packet_by_path_tag()"<<std::endl;

        return true;
    }

    /*void RdmaSmartFlowRouting::add_conga_tag_by_pit_entry(Ptr<Packet> packet, PathData *pitEntry)
    {
        if (pitEntry == 0)
        {
            return;
        }

        Ipv4SmartFlowCongaTag congaTag;
        congaTag.SetPathId(pitEntry->pid);
        congaTag.SetDre(pitEntry->pathDre);
        packet->AddPacketTag(congaTag);
        // std::cout << "SrcSwitch PiggyBack congestion degree: " << "    ";
        // std::cout << "Node: " << get_node_id() << "    ";
        // std::cout << "Time: " << Simulator::Now().GetNanoSeconds() << "    ";
        // std::cout << "Pid: " << congaTag.get_path_id() << "    ";
        // std::cout << "QLen: " << congaTag.GetDre() << "    ";
        // std::cout << "\n";
    }*/

    void RdmaSmartFlowRouting::add_latency_tag_by_pit_entries(Ptr<Packet> packet, std::vector<PathData *> &pitEntries)
    {
        // NS_LOG_INFO ("############ Fucntion: add_latency_tag_by_pit_entries() ############");
        uint32_t pathNum = pitEntries.size();
        if (pathNum == 0 || m_piggyLatencyCnt == 0)
        {
            NS_LOG_INFO("No need to add latency tag since pathNum=" << pathNum << ", m_piggyCnt=" << m_piggyLatencyCnt);
            return;
        }
        else
        {
            for (uint32_t i = 0; i < pathNum; i++)
            {
                if (i + 1 > m_piggyLatencyCnt)
                {
                    break;
                }
                if (i == 0)
                {
                    Ipv4SmartFlowLatencyTag<0> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 1)
                {
                    Ipv4SmartFlowLatencyTag<1> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 2)
                {
                    Ipv4SmartFlowLatencyTag<2> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 3)
                {
                    Ipv4SmartFlowLatencyTag<3> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 4)
                {
                    Ipv4SmartFlowLatencyTag<4> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 5)
                {
                    Ipv4SmartFlowLatencyTag<5> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 6)
                {
                    Ipv4SmartFlowLatencyTag<6> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 7)
                {
                    Ipv4SmartFlowLatencyTag<7> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 8)
                {
                    Ipv4SmartFlowLatencyTag<8> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
                else if (i == 9)
                {
                    Ipv4SmartFlowLatencyTag<9> latencyTag;
                    latencyTag.set_data_by_pit_entry(pitEntries[i]);
                    packet->AddPacketTag(latencyTag);
                }
            }
        }
        return;
    }

    void RdmaSmartFlowRouting::update_PIT_after_adding_path_tag(PathData *piggyBackPitEntry)
    {
        NS_LOG_INFO("############ Fucntion: update_PIT_after_adding_path_tag() ############");
        if (m_pathSelStrategy == PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY)
        {
            piggyBackPitEntry->tsProbeLastSend = Simulator::Now();
        }
    }

    void RdmaSmartFlowRouting::update_PST_after_adding_path_tag(pstEntryData *pstEntry, PathData *forwardPitEntry)
    {
        NS_LOG_INFO("############ Fucntion: update_PST_after_adding_path_tag() ############");
        if (m_pathSelStrategy == PATH_SELECTION_FLOWLET_STATEGY)
        {
            uint32_t pathCnt = pstEntry->paths.size();
            for (uint32_t i = 0; i < pathCnt; i++)
            {
                if (pstEntry->paths[i] == forwardPitEntry->pid)
                {
                    pstEntry->lastSelectedPathIdx = i;
                    pstEntry->lastSelectedTimeInNs = Simulator::Now().GetNanoSeconds();
                    return;
                }
            }
            NS_LOG_INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Error in update_PST_after_adding_path_tag due to flowlet strategy");
        }
        else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_PACKET_STRATEGY)
        {
            if (pstEntry->lastSelectedPathIdx == DEFAULT_PATH_INDEX)
            {
                pstEntry->lastSelectedPathIdx = 0;
            }
            else
            {
                pstEntry->lastSelectedPathIdx = (pstEntry->lastSelectedPathIdx + 1) % pstEntry->pathNum;
            }
        }
    }

    uint32_t RdmaSmartFlowRouting::update_PIT_after_piggybacking(std::vector<PathData *> &piggyBackPitEntries)
    {
        NS_LOG_INFO("############ Fucntion: update_PIT_after_piggybacking() ############");
        uint32_t pathNum = piggyBackPitEntries.size();
        for (uint32_t i = 0; i < pathNum; i++)
        {
            piggyBackPitEntries[i]->tsLatencyLastSend = Simulator::Now();
        }
        return pathNum;
    }

    Ipv4SmartFlowPathTag RdmaSmartFlowRouting::construct_path_tag(uint32_t selectedPathId)
    {
        NS_LOG_INFO("############ Fucntion: construct_path_tag() ############");
        Ipv4SmartFlowPathTag pathTag;
        pathTag.SetPathId(selectedPathId);
        pathTag.set_hop_idx(0);
        Time now = Simulator::Now();
        pathTag.SetTimeStamp(now);
        pathTag.SetDre(0);
        return pathTag;
    }

    PathData *RdmaSmartFlowRouting::get_the_smallest_latency_path(std::vector<PathData *> &pitEntries)
    {
        NS_LOG_INFO("############ Fucntion: get_the_smallest_latency_path() ############");
        if (pitEntries.size() == 0)
        {
            return 0;
        }
        std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_latency);
        return pitEntries[0];
    }

    PathData *RdmaSmartFlowRouting::get_the_oldest_measured_path(std::vector<PathData *> &pitEntries)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_oldest_measured_path() ############");
        if (pitEntries.size() == 0)
        {
            return 0;
        }
        std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_Generation_time);
        return pitEntries[0];
    }

    PathData *RdmaSmartFlowRouting::get_the_random_path(std::vector<PathData *> &pitEntries)
    {
        NS_LOG_INFO("############ Fucntion: get_the_random_path() ############");
        if (pitEntries.size() == 0)
        {
            return 0;
        }
        PathData *bestPitEntry = pitEntries[std::rand() % pitEntries.size()];
        return bestPitEntry;
    }

    /* PathData *RdmaSmartFlowRouting::get_the_flowlet_path(pstEntryData *pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header)
     {
         NS_LOG_INFO("get_the_flowlet_path()");
         std::string flowStr = hashing_flow_with_5_tuple_to_string(packet, header);
         NS_LOG_INFO("Flow INFO : " << flowStr);
         std::map<std::string, flet_entry_t>::iterator it;
         it = m_fletTbl.find(flowStr);
         PathData *bestPitEntry = 0;
         Time curTime = Simulator::Now();
         uint32_t curTimeInNs = curTime.GetNanoSeconds();
         uint32_t curSelectedPathIdx = std::rand() % pitEntries.size();
         if (it == m_fletTbl.end())
         {
             NS_LOG_INFO("New Flow");
             bestPitEntry = pitEntries[curSelectedPathIdx];
             flet_entry_t fletEnty = flet_entry_t();
             fletEnty.lastSelectedPitEntry = bestPitEntry;
             fletEnty.lastSelectedTimeInNs = curTimeInNs;
             m_fletTbl[flowStr] = fletEnty;
         }
         else
         {
             NS_LOG_INFO("Exist Flow");
             uint32_t gap = curTimeInNs - it->second.lastSelectedTimeInNs;
             NS_LOG_INFO("Flowlet Timout In Nano Second : " << m_flowletTimoutInNs << ", curGapInNs: " << gap);
             if (gap > m_flowletTimoutInNs)
             {
                 NS_LOG_INFO("New Flowlet, changes the path");
                 bestPitEntry = pitEntries[curSelectedPathIdx];
                 it->second.lastSelectedPitEntry = bestPitEntry;
                 it->second.lastSelectedTimeInNs = curTimeInNs;
             }
             else
             {
                 NS_LOG_INFO("Same Flowlet, maintain the previous path");
                 bestPitEntry = it->second.lastSelectedPitEntry;
                 it->second.lastSelectedTimeInNs = curTimeInNs;
             }
         }
         return bestPitEntry;
     }*/

    PathData *RdmaSmartFlowRouting::get_the_highest_priority_path(std::vector<PathData *> &pitEntries)
    {
        PathData *bestPitEntry = 0;
        std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_decrease_order_of_priority);
        bestPitEntry = pitEntries[0];
        return bestPitEntry;
    }

    /*PathData *RdmaSmartFlowRouting::get_the_best_forwarding_path(std::vector<PathData *> &pitEntries, pstEntryData *pstEntry, Ptr<const Packet> packet, const Ipv4Header &header)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_best_forwarding_path() ############");
        PathData *bestPitEntry = 0;
        if (m_pathSelStrategy == PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY)
        {
            bestPitEntry = get_the_smallest_latency_path(pitEntries); // smartflow
            // bestPitEntry->tsProbeLastSend = Simulator::Now();
        }
        else if (m_pathSelStrategy == PATH_SELECTION_RANDOM_STRATEGY)
        { // hashflow
            bestPitEntry = get_the_random_path(pitEntries);
        }
        else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_PACKET_STRATEGY)
        { // rbnflow
            bestPitEntry = get_the_round_robin_path_for_packet(pstEntry, pitEntries);
        }
        else if (m_pathSelStrategy == PATH_SELECTION_FLOW_HASH_STRATEGY)
        { // ecmp
            bestPitEntry = get_the_hashing_path(pitEntries, packet, header);
        }
        else if (m_pathSelStrategy == PATH_SELECTION_PRIORITY_FIRST_STRATEGY)
        { // minflow
            bestPitEntry = get_the_highest_priority_path(pitEntries);
        }
        else if (m_pathSelStrategy == PATH_SELECTION_FLOWLET_STATEGY)
        {
            bestPitEntry = get_the_flowlet_path(pstEntry, pitEntries, packet, header);
        }
        else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_FLOW_STRATEGY)
        {
            bestPitEntry = get_the_round_robin_path_for_flow(pstEntry, pitEntries, packet, header);
        }
        else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_HYBRID_STRATEGY)
        {
            bestPitEntry = get_the_round_robin_path_for_hybird(pstEntry, pitEntries, packet, header);
        }
        else if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY)
        {

            bestPitEntry = get_the_least_congested_path(pstEntry, pitEntries, packet, header); // conga
        }
        return bestPitEntry;
    }*/

    std::vector<PathData *> RdmaSmartFlowRouting::get_the_best_piggyback_paths(std::vector<PathData *> &pitEntries)
    {
        NS_LOG_INFO("############ Fucntion: get_the_best_reverse_paths() ############");
        std::vector<PathData *> bestPitEntries;
        bestPitEntries.clear();
        // NS_LOG_INFO("The results after apply piggyback strategy=" << m_piggybackStrategy << " are:{0:latencyFirst},  {1:gentFirst}, {2:sentFirst}");
        if (m_piggybackStrategy == PIGGY_BACK_SMALL_LATENCY_FIRST_STRATEGY)
        {
            std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_latency);
        }
        else if (m_piggybackStrategy == PIGGY_BACK_SMALL_GENT_TIME_FIRST_STRATEGY)
        {
            std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_Generation_time);
        }
        else if (m_piggybackStrategy == PIGGY_BACK_SMALL_SENT_TIME_FIRST_STRATEGY)
        {
            std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_Sent_time);
        }
        else
        {
            std::cout << "Error in get_the_best_piggyback_paths() since m_probeStrategy=" << m_piggybackStrategy << std::endl;
            return bestPitEntries;
        }
        uint32_t allCnt = pitEntries.size();
        if (allCnt <= m_piggyLatencyCnt)
        {
            bestPitEntries.assign(pitEntries.begin(), pitEntries.end());
        }
        else
        {
            bestPitEntries.assign(pitEntries.begin(), pitEntries.begin() + m_piggyLatencyCnt);
        }
        return bestPitEntries;
    }

    uint32_t RdmaSmartFlowRouting::get_the_expired_paths(std::vector<PathData *> &allPitEntries, std::vector<PathData *> &expiredPitEntries)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_expired_paths() ############");
        expiredPitEntries.clear();
        int64_t curTime = Simulator::Now().GetNanoSeconds();
        for (auto e : allPitEntries)
        {
            if ((curTime - e->tsGeneration.GetNanoSeconds()) > (e->theoreticalSmallestLatencyInNs * 2 + RDMA_LINK_LATENCY_IN_NANOSECOND))
            {
                expiredPitEntries.push_back(e);
            }
        }
        return expiredPitEntries.size();
    }

    uint32_t RdmaSmartFlowRouting::get_the_potential_paths(PathData *bestPitEntry, std::vector<PathData *> &allPitEntries, std::vector<PathData *> &potentialPitEntries)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_potential_paths() ############");
        potentialPitEntries.clear();
        for (auto e : allPitEntries)
        {
            if (bestPitEntry->latency > e->theoreticalSmallestLatencyInNs)
            {
                potentialPitEntries.push_back(e);
            }
        }
        return potentialPitEntries.size();
    }

    uint32_t RdmaSmartFlowRouting::get_the_probe_paths(std::vector<PathData *> &expiredPitEntries, std::vector<PathData *> &probePitEntries)
    {
        NS_LOG_INFO("############ Fucntion: get_the_probe_paths() ############");
        probePitEntries.clear();
        int64_t curTime = Simulator::Now().GetNanoSeconds();
        for (auto e : expiredPitEntries)
        {
            if ((curTime - e->tsProbeLastSend.GetNanoSeconds()) > (e->theoreticalSmallestLatencyInNs * 2 + RDMA_LINK_LATENCY_IN_NANOSECOND))
            {
                probePitEntries.push_back(e);
            }
        }
        return probePitEntries.size();
    }

    PathData *RdmaSmartFlowRouting::get_the_best_probing_path(std::vector<PathData *> &pitEntries)
    {
        // NS_LOG_INFO("The results after apply probing strategy=" << m_probeStrategy << " are:{0:latencyFirst}, {1: gentFirst}, {2:random}");

        // NS_LOG_INFO ("############ Fucntion: get_the_best_probing_path() ############");
        if (pitEntries.size() == 0)
        {
            return 0;
        }
        if (m_probeStrategy == PROBE_SMALL_LATENCY_FIRST_STRATEGY)
        {
            return get_the_smallest_latency_path(pitEntries);
        }
        else if (m_probeStrategy == PROBE_RANDOM_STRATEGY)
        { // hashflow
            return get_the_random_path(pitEntries);
        }
        else if (m_probeStrategy == PROBE_SMALL_GENERATION_TIME_FIRST_STRATEGY)
        {
            return get_the_oldest_measured_path(pitEntries);
        }
        return 0;
    }

    void RdmaSmartFlowRouting::initialize()
    {
        // highestPriorityPathIdx
        for (auto &pstEntry : m_pathSelTbl)
        {
            std::vector<PathData *> pitEntries = batch_lookup_PIT(pstEntry.second.paths);
            std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_decrease_order_of_priority);
            uint32_t highestPriorityPathIdx = 0;
            for (uint32_t i = 1; i < pstEntry.second.paths.size(); i++)
            {
                if (pstEntry.second.paths[i] == pitEntries[0]->pid)
                {
                    highestPriorityPathIdx = i;
                    break;
                }
            }
            pstEntry.second.highestPriorityPathIdx = highestPriorityPathIdx;
            pstEntry.second.lastSelectedPathIdx = DEFAULT_PATH_INDEX;
            pstEntry.second.lastPiggybackPathIdx = 0;
        }
        for (auto &pitEntry : m_nexthopSelTbl)
        {
            pitEntry.second.theoreticalSmallestLatencyInNs = pitEntry.second.portSequence.size() * RDMA_LINK_LATENCY_IN_NANOSECOND;
            pitEntry.second.pathDre = 0; // to be modified
        }
        // m_nodeId
        m_nodeId = m_switch_id;
        for (size_t i = 1; i < CONGA_SW_PORT_NUM; i++)
        {
            m_XMap[i] = 0;
        }
        // lastSelectedPathIdx
    }

    std::vector<PathData *> RdmaSmartFlowRouting::get_the_newly_measured_paths(std::vector<PathData *> &pitEntries)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_newly_measured_paths() ############");
        std::vector<PathData *> candidatePaths;
        candidatePaths.clear();
        uint32_t pathNum = pitEntries.size();
        for (uint32_t i = 0; i < pathNum; i++)
        {
            Time tsGeneration = pitEntries[i]->tsGeneration;
            Time tsLatencyLastSend = pitEntries[i]->tsLatencyLastSend;
            if (tsGeneration.GetNanoSeconds() > tsLatencyLastSend.GetNanoSeconds())
            {
                candidatePaths.push_back(pitEntries[i]);
            }
        }
        return candidatePaths;
    }

    uint32_t RdmaSmartFlowRouting::install_PIT(std::map<uint32_t, PathData> &pit)
    {
        set_PIT(pit);
        return pit.size();
    }

    uint32_t RdmaSmartFlowRouting::install_PST(std::map<HostId2PathSeleKey, pstEntryData> &pst)
    {
        set_PST(pst);
        return pst.size();
    }

    uint32_t RdmaSmartFlowRouting::install_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &vmt)
    {
        set_SMT(vmt);
        return vmt.size();
    }

    void RdmaSmartFlowRouting::update_PIT_by_latency_data(LatencyData &latencyData)
    {
        // NS_LOG_INFO ("############ Fucntion: update_PIT_by_latency_data() ############");
        uint32_t pid = latencyData.latencyInfo.first;
        uint32_t newLatency = latencyData.latencyInfo.second;
        Time newLatencyGeneration = latencyData.tsGeneration;
        PathData *pitEntry = lookup_PIT(pid);
        // uint32_t oldLatency = pitEntry->latency;
        Time oldLatencyGeneration = pitEntry->tsGeneration;

        //  NS_LOG_INFO (" changes the path info <pathId=" << pid << "> : " <<
        //              "latency=" << oldLatency <<" ==> "<< newLatency << ", "
        //              "tsGeneration=" << oldLatencyGeneration.GetNanoSeconds()<< "==>" << newLatencyGeneration.GetNanoSeconds() << ">");

        //  std::cout<<"Src ToR uses Latency Tag to change the path info "<<" "
        //              "<pathId=" << latencyData.latencyInfo.first << ", " <<
        //              "latency=" << m_nexthopSelTbl[latencyData.latencyInfo.first].latency <<
        //              " ==> "<< latencyData.latencyInfo.second << ", "
        //              "tsGeneration=" << m_nexthopSelTbl[latencyData.latencyInfo.first].tsGeneration.GetNanoSeconds()<<
        //              "==>" << latencyData.tsGeneration.GetNanoSeconds() << ">"<< std::endl;

        if (newLatencyGeneration > oldLatencyGeneration)
        {
            pitEntry->latency = newLatency;
            pitEntry->tsGeneration = newLatencyGeneration;
        }
        else
        {
            NS_LOG_INFO("!!!!!!!!!!!!!!SPECIAL CASE: older LatencyINFO overrides the newly one!!!!!!!!!!!!!!!");
        }
    }

    void RdmaSmartFlowRouting::update_PIT_by_latency_tag(Ptr<Packet> &packet)
    {
        // NS_LOG_INFO ("############ Fucntion: update_PIT_by_latency_tag() ############");
        for (uint32_t i = 0; i < m_piggyLatencyCnt; i++)
        {
            if (i == 0)
            {
                Ipv4SmartFlowLatencyTag<0> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 1)
            {
                Ipv4SmartFlowLatencyTag<1> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 2)
            {
                Ipv4SmartFlowLatencyTag<2> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 3)
            {
                Ipv4SmartFlowLatencyTag<3> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 4)
            {
                Ipv4SmartFlowLatencyTag<4> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 5)
            {
                Ipv4SmartFlowLatencyTag<5> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 6)
            {
                Ipv4SmartFlowLatencyTag<6> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 7)
            {
                Ipv4SmartFlowLatencyTag<7> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 8)
            {
                Ipv4SmartFlowLatencyTag<8> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
            else if (i == 9)
            {
                Ipv4SmartFlowLatencyTag<9> latencyTag;
                if (packet->PeekPacketTag(latencyTag))
                {
                    LatencyData latencyData = latencyTag.GetLatencyData();
                    update_PIT_by_latency_data(latencyData);
                }
                else
                {
                    break;
                }
            }
        }
        return;
    }

    void RdmaSmartFlowRouting::update_PIT_by_path_tag(Ipv4SmartFlowPathTag &pathTag, PathData *&pitEntry)
    {
        // NS_LOG_INFO ("############ Fucntion: update_PIT_by_path_tag() ############");

        /*if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
            // std::cout << "DstSwitch Record Reverse congestion degree: " << "    ";
            // std::cout << "Node: " << get_node_id() << "    ";
            // std::cout << "Time: " << Simulator::Now().GetNanoSeconds() << "    ";
            // std::cout << "Pid: " << pitEntry->pid << "    ";
            // std::cout << "QLen: " << pitEntry->SetDre << " --> " << pathTag.GetDre() << "    ";
            // std::cout << "\n";
            // NS_LOG_INFO("Node " << m_nodeId << " updates PIT from INT");
            // std::cout << "Node " << m_nodeId << " updates reverse PIT from INT" << std::endl;

            // pitEntry->print();
            uint32_t dre = pathTag.GetDre();
            pitEntry->pathDre = dre;
            pitEntry->tsGeneration = Simulator::Now();
            // pitEntry->print();
            return ;
        }*/

        Time curTime = Simulator::Now();
        Time oldTime = pathTag.GetTimeStamp();
        uint32_t newLatency = (uint32_t)(curTime.GetNanoSeconds() - oldTime.GetNanoSeconds());
        pitEntry->latency = newLatency;
        pitEntry->tsGeneration = curTime;
        return;
    }

    /*void RdmaSmartFlowRouting::update_PIT_by_conga_tag(Ipv4SmartFlowCongaTag &congaTag)
    {
        // NS_LOG_INFO ("############ Fucntion: update_PIT_by_path_tag() ############");
        uint32_t pid = congaTag.get_path_id();
        uint32_t pathDre = congaTag.GetDre();
        PathData *pitEntry = lookup_PIT(pid);
        // std::cout << "SrcSwitch Update congestion degree: " << "    ";
        // std::cout << "Node: " << get_node_id() << "    ";
        // std::cout << "Time: " << Simulator::Now().GetNanoSeconds() << "    ";
        // std::cout << "Pid: " << pid << "    ";
        // std::cout << "QLen: " << pitEntry->SetDre << " --> " << SetDre << "    ";
        // std::cout << "\n";
        // NS_LOG_INFO("Node " << m_nodeId << " updates PIT from piggyback");
        // std::cout << "Node " << m_nodeId << " updates PIT from piggyback" << std::endl;
        // pitEntry->print();
        pitEntry->pathDre = pathDre;
        pitEntry->tsGeneration = Simulator::Now();
        // pitEntry->print();

        return;
    }*/

    void RdmaSmartFlowRouting::update_PIT_by_probe_tag(Ipv4SmartFlowProbeTag &probeTag)
    {
        // NS_LOG_INFO ("############ Fucntion: update_PIT_by_probe_tag() ############");
        uint32_t pid, newLatency;
        probeTag.get_path_info(pid, newLatency);
        PathData *pitEntry = lookup_PIT(pid);
        pitEntry->latency = newLatency;
        pitEntry->tsGeneration = Simulator::Now();
        return;
    }

    void RdmaSmartFlowRouting::receive_probe_packet(Ipv4SmartFlowProbeTag &probeTag)
    {
        NS_LOG_INFO("############ Fucntion: receive_probe_packet() ############");
        /*if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
            std::cout<< "un expected probe packet in conga" << std::endl;
            return ;
        }*/

        update_PIT_by_probe_tag(probeTag);
        return;
    }

    void RdmaSmartFlowRouting::update_path_tag(Ptr<Packet> &packet, Ipv4SmartFlowPathTag &pathTag)
    {
        // NS_LOG_INFO ("############ Fucntion: update_path_tag() ############");
        uint32_t curHopIdx = pathTag.get_the_current_hop_index();
        pathTag.set_hop_idx(curHopIdx + 1);
        packet->ReplacePacketTag(pathTag);
        return;
    }

    Ptr<Packet> RdmaSmartFlowRouting::construct_probe_packet(Ptr<Packet> &pkt, CustomHeader &ch)
    {
        Ptr<Packet> probePacket = Create<Packet>(PROBE_DEFAULT_PKT_SIZE_IN_BYTE);

        /*Ptr<Packet> p = Create<Packet>(0);
        Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
        PauseHeader pauseh((type == 0 ? m_pausetime : 0), m_queue->GetNBytes(qIndex), qIndex);
        p->AddHeader(pauseh);
        Ipv4Header ipv4h;  // Prepare IPv4 header
        ipv4h.SetProtocol(0xFE);
        ipv4h.SetSource(m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal());
        ipv4h.SetDestination(Ipv4Address("255.255.255.255"));
        ipv4h.SetPayloadSize(p->GetSize());
        ipv4h.SetTtl(1);
        ipv4h.SetIdentification(x->GetValue(0, 65536));
        p->AddHeader(ipv4h);
        AddHeader(p, 0x800);
        CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
        p->PeekHeader(ch);
        */

        // 添加 SeqTsHeader
        SeqTsHeader seqTs;
        seqTs.SetSeq(0); // 设置初始序列号
        probePacket->AddHeader(seqTs);
        // 添加 UdpHeader
        UdpHeader udpHeader;
        udpHeader.SetDestinationPort(ch.udp.dport); //
        udpHeader.SetSourcePort(ch.udp.sport);      //
        probePacket->AddHeader(udpHeader);

        /*Ipv4Header ipv4h;  // Prepare IPv4 header
        ipv4h.SetProtocol(0xFE);
        ipv4h.SetSource(m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal());
        ipv4h.SetDestination(Ipv4Address("255.255.255.255"));
        ipv4h.SetPayloadSize(p->GetSize());
        ipv4h.SetTtl(1);
        ipv4h.SetIdentification(x->GetValue(0, 65536));
        p->AddHeader(ipv4h);
        AddHeader(p, 0x800);*/

        // 添加 Ipv4Header
        Ipv4Header ipHeader;
        ipHeader.SetSource(Ipv4Address(ch.sip));
        ipHeader.SetDestination(Ipv4Address(ch.dip));
        ipHeader.SetProtocol(0x11); // UDP protocol number
        ipHeader.SetPayloadSize(probePacket->GetSize());
        ipHeader.SetTtl(64);
        ipHeader.SetTos(0);
        ipHeader.SetIdentification(50);
        probePacket->AddHeader(ipHeader);

        // 添加 PppHeader
        PppHeader ppp;
        ppp.SetProtocol(0x0021); // EtherToPpp(0x800), see point-to-point-net-device.cc
        probePacket->AddHeader(ppp);

        return probePacket;
    }

    Ipv4SmartFlowProbeTag RdmaSmartFlowRouting::construct_probe_tag_by_path_id(uint32_t expiredPathId)
    {
        // NS_LOG_INFO ("############ Fucntion: construct_probe_tag_by_path_id() ############");
        Ipv4SmartFlowProbeTag probeTag;
        probeTag.Initialize(expiredPathId);
        return probeTag;
    }

    pstEntryData *RdmaSmartFlowRouting::lookup_PST(HostId2PathSeleKey &pstKey)
    {
        // NS_LOG_INFO ("############ Fucntion: lookup_PST() ############");
        std::map<HostId2PathSeleKey, pstEntryData>::iterator it;
        it = m_pathSelTbl.find(pstKey);
        if (it == m_pathSelTbl.end())
        {
            std::cout << "Error in lookup_PST() since Cannot match any entry in PST for the Key: (";
            std::cout << pstKey.selfHostId;
            std::cout << ", " << pstKey.dstHostId << ")" << std::endl;
            return 0;
        }
        else
        {
            return &(it->second);
        }
    }

    pdt_entry_t *RdmaSmartFlowRouting::lookup_PDT(HostId2PathSeleKey &pstKey)
    {
        NS_LOG_INFO("############ Fucntion: lookup_PDT() ############");
        auto it = m_pathDecTbl.find(pstKey);
        if (it == m_pathDecTbl.end())
        {
            NS_LOG_INFO("No matching entry in PDT for the following pstKey");
            // pstKey.print();
            return 0;
        }
        else
        {
            return &(it->second);
        }
    }

    PathData *RdmaSmartFlowRouting::lookup_PIT(uint32_t pieKey)
    {
        // NS_LOG_INFO ("############ Fucntion: lookup_PIT() ############");
        std::map<uint32_t, PathData>::iterator it;
        it = m_nexthopSelTbl.find(pieKey);
        if (it == m_nexthopSelTbl.end())
        {
            std::cout << "Error in lookup_PIT() since Cannot match any entry in PIT for the Key: ";
            std::cout << pieKey;
            std::cout << std::endl;
            return 0;
        }
        else
        {
            return &(it->second);
        }
    }
    std::vector<PathData *> RdmaSmartFlowRouting::batch_lookup_PIT(std::vector<uint32_t> &pids)
    {
        // NS_LOG_INFO ("############ Fucntion: batch_lookup_PIT() ############");
        std::vector<PathData *> pitEntries;
        pitEntries.clear();
        uint32_t pathNum = pids.size();
        for (uint32_t i = 0; i < pathNum; i++)
        {
            PathData *curPitEntry = lookup_PIT(pids[i]);
            pitEntries.push_back(curPitEntry);
        }
        return pitEntries;
    }

    // uint32_t flowId = 0;
    // Hasher m_hasher;
    // m_hasher.clear();
    // TcpHeader tcpHeader;
    // packet->PeekHeader(tcpHeader);
    // std::ostringstream oss;
    // oss << tcpHeader.GetDestinationPort() << tcpHeader.GetSourcePort()//获取目标端口>获取源端口>
    //     << header.GetSource() << header.GetDestination() << header.GetProtocol();//获取源ip、目的ip、传输层协议
    // std::string data = oss.str();
    // flowId = m_hasher.GetHash32(data);
    // return flowId;

    /*void RdmaSmartFlowRouting::record_reorder_at_dst_tor(Ptr<Packet> &pkt, const Ipv4Header &header)
    {
        // NS_LOG_INFO ("############ Fucntion: check_reorder_info_in_dst_switch() ############");
        TcpHeader tcpHeader;
        if (pkt->PeekHeader(tcpHeader) == 0)
        {
            // NS_LOG_INFO("This Packet does not have TCP header");
            return;
        }
        std::string flowStr = hashing_flow_with_5_tuple_to_string(pkt, header);
        // NS_LOG_INFO("5-tuple Info of the packet :" << flowStr);

        uint32_t tcpHeaderLengthInByte = ((uint32_t)tcpHeader.GetLength()) * 4;
        uint32_t pktSizeInByte = pkt->GetSize();
        uint32_t tcpSeqNum = tcpHeader.GetSequenceNumber().GetValue();
        // uint32_t expectedSeqNum = tcpSeqNum + pktSizeInByte - tcpHeaderLengthInByte;
        uint32_t tcpPayloadInByte = pktSizeInByte - tcpHeaderLengthInByte;
        std::string flagStr = TcpHeader::FlagsToString(tcpHeader.GetFlags());

        // NS_LOG_INFO("Packet Size without ipv4 header but With TCP header in byte: " << pktSizeInByte);
        // NS_LOG_INFO("TCP Header Size in Byte: " << tcpHeaderLengthInByte);
        // NS_LOG_INFO("TCP Flags: " << flagStr);
        // NS_LOG_INFO("curSeqNum: " << tcpSeqNum);
        // NS_LOG_INFO("Packet Payload In Byte for TCP Header: " << tcpPayloadInByte);
        // NS_LOG_INFO("expectedSeqNum: " << expectedSeqNum);

        auto it = m_reorderTable.find(flowStr);
        if (flagStr == "SYN")
        {
            // NS_LOG_INFO("The First handshake packet of the new flow sent from the sender to the receiver");
            reorder_entry_t reorderEntry = reorder_entry_t();
            reorderEntry.flag = true;
            reorderEntry.seqs.clear();
            m_reorderTable[flowStr] = reorderEntry;
        }
        else if (flagStr == "SYN|ACK")
        {
            // NS_LOG_INFO("The Second handshake packet of the new flow sent from the receiver to the sender");
            reorder_entry_t reorderEntry = reorder_entry_t();
            reorderEntry.flag = false;
            reorderEntry.seqs.clear();
            m_reorderTable[flowStr] = reorderEntry;
        }
        else if (flagStr == "FIN|ACK")
        {
            // NS_LOG_INFO("The teardown packet");
            if (it == m_reorderTable.end())
            {
                std::cout << "Error in record_reorder_at_dst_tor() since the un-recorded flow" << std::endl;
            }
            else if (it->second.flag == false)
            {
                NS_LOG_INFO("The Second teardown packet from the receiver to the sender");
                // it->second.print();
            }
            else
            {
                // NS_LOG_INFO("The First teardown packet with normal payload from the sender to the receiver");
                it->second.seqs.push_back(tcpSeqNum);
                // it->second.print();
            }
        }
        else if (flagStr == "ACK")
        {
            // NS_LOG_INFO("The Mid packet of the flow");
            if (it == m_reorderTable.end())
            {
                std::cout << "Error in record_reorder_at_dst_tor() since the un-recorded flow" << std::endl;
            }
            else if (it->second.flag == false)
            {
                NS_LOG_INFO("From the receiver to the sender");
                // it->second.print();
            }
            else
            {
                if ((tcpPayloadInByte == 0) && (pktSizeInByte == 32) && (tcpSeqNum == 1))
                {
                    NS_LOG_INFO("The third handshake packet of the flow sent from the sender to the receiver");
                }
                else
                {
                    it->second.seqs.push_back(tcpSeqNum);
                }
                // it->second.print();
            }
        }
        return;
    }*/

    void RdmaSmartFlowRouting::receive_normal_packet(Ptr<Packet> &pkt, Ipv4SmartFlowPathTag &pathTag, PathData *&pitEntry)
    {
        // NS_LOG_INFO ("############ Fucntion: receive_normal_packet() ############");
        /*if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
            Ipv4SmartFlowCongaTag congaTag;
            if (pkt->PeekPacketTag(congaTag)) { // reaching the dst ToR switch
                update_PIT_by_conga_tag(congaTag);
            }
            update_PIT_by_path_tag(pathTag, pitEntry);
            return ;

        }*/

        if ((m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY) && (m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY))
        {
            return;
        }
        update_PIT_by_path_tag(pathTag, pitEntry);
        update_PIT_by_latency_tag(pkt);
        return;
    }

    void RdmaSmartFlowRouting::add_probe_tag_by_path_id(Ptr<Packet> packet, uint32_t expiredPathId)
    {
        // NS_LOG_INFO ("############ Fucntion: add_probe_tag_by_path_id() ############");
        Ipv4SmartFlowProbeTag probeTag = construct_probe_tag_by_path_id(expiredPathId);
        packet->AddPacketTag(probeTag);
        return;
    }

    void RdmaSmartFlowRouting::update_PIT_after_probing(PathData *pitEntry)
    {
        // NS_LOG_INFO ("############ Fucntion: update_PIT_after_probing() ############");
        pitEntry->tsProbeLastSend = Simulator::Now();
        return;
    }

    void RdmaSmartFlowRouting::add_path_tag_by_path_id(Ptr<Packet> packet, uint32_t pid)
    {
        // NS_LOG_INFO ("############ Fucntion: add_path_tag_by_path_id() ############");
        Ipv4SmartFlowPathTag pathTag = construct_path_tag(pid);
        packet->AddPacketTag(pathTag);
        return;
    }

    uint32_t RdmaSmartFlowRouting::get_the_path_length_by_path_id(const uint32_t pathId, PathData *&pitEntry)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_path_length_by_path_id() ############");
        pitEntry = lookup_PIT(pathId);
        return pitEntry->portSequence.size();
    }

    bool RdmaSmartFlowRouting::reach_the_last_hop_of_path_tag(Ipv4SmartFlowPathTag &smartFlowTag, PathData *&pitEntry)
    {
        // NS_LOG_INFO ("############ Fucntion: reach_the_last_hop_of_path_tag() ############");
        uint32_t pathId = smartFlowTag.get_path_id();
        uint32_t pathSize = get_the_path_length_by_path_id(pathId, pitEntry);
        uint32_t hopIdx = smartFlowTag.get_the_current_hop_index();
        if (hopIdx == pathSize)
        {
            // NS_LOG_INFO ("Reaching the Dst ToR");
            return true;
        }
        else
        {
            // NS_LOG_INFO ("Is " << hopIdx << "/" << pathSize << " hops");
            return false;
        }
    }
    bool RdmaSmartFlowRouting::reach_the_DstToR_last_hop_of_path_tag(Ipv4SmartFlowPathTag &smartFlowTag, PathData *&pitEntry)
    {
        NS_LOG_INFO("############ Fucntion: reach_the_last_hop_of_path_tag() ############");
        uint32_t pathId = smartFlowTag.get_path_id();
        uint32_t pathSize = get_the_path_length_by_path_id(pathId, pitEntry);
        uint32_t hopIdx = smartFlowTag.get_the_current_hop_index();

        if (hopIdx == (pathSize - 1))
        {
            NS_LOG_INFO("Reaching the Dst ToR");
            return true;
        }
        else
        {
            NS_LOG_INFO("Is " << hopIdx << "/" << pathSize << " hops");
            return false;
        }
    }

    bool RdmaSmartFlowRouting::exist_path_tag(Ptr<Packet> packet, Ipv4SmartFlowPathTag &pathTag)
    {
        NS_LOG_INFO("############ Fucntion: exist_path_tag() ############");
        if (packet->PeekPacketTag(pathTag))
        { // reaching the src ToR switch
            NS_LOG_INFO("Path Tag indeed exits");
            return true;
        }
        NS_LOG_INFO("Path Tag does not exit");
        return false;
    }

    bool RdmaSmartFlowRouting::exist_probe_tag(Ptr<Packet> packet, Ipv4SmartFlowProbeTag &probeTag)
    {
        // NS_LOG_INFO ("############ Fucntion: exist_probe_tag() ############");
        if (packet->PeekPacketTag(probeTag))
        { // reaching the src ToR switch
            // NS_LOG_INFO ("Probe Tag indeed exits");
            return true;
        }
        // NS_LOG_INFO ("Probe Tag does not exit");
        return false;
    }

    uint32_t RdmaSmartFlowRouting::get_egress_port_id_by_path_tag(Ipv4SmartFlowPathTag &smartFlowTag)
    {
        // NS_LOG_INFO ("############ Fucntion: get_egress_port_id_by_path_tag() ############");
        uint32_t pathId = smartFlowTag.get_path_id();
        uint32_t hopIdx = smartFlowTag.get_the_current_hop_index();
        PathData *pitEntry = lookup_PIT(pathId);
        return pitEntry->portSequence[hopIdx + 1];
    }

    std::vector<PathData *> RdmaSmartFlowRouting::get_the_piggyback_pit_entries(uint32_t srcToRId, uint32_t dstHostId)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_piggyback_pit_entries() ############");
        HostId2PathSeleKey pstKey(srcToRId, dstHostId);
        pstEntryData *pstEntry = lookup_PST(pstKey);
        std::vector<PathData *> allPitEntries = batch_lookup_PIT(pstEntry->paths);
        std::vector<PathData *> freshPitEntries = get_the_newly_measured_paths(allPitEntries);
        std::vector<PathData *> piggybackPitEntries = get_the_best_piggyback_paths(freshPitEntries);
        return piggybackPitEntries;
    }

    /* std::vector<uint32_t> RdmaSmartFlowRouting::get_dre_of_egress_ports(std::vector<uint32_t> &ports)
     {
         uint32_t n = ports.size();
         std::vector<uint32_t> dres(n);
         for (size_t i = 0; i < n; i++)
         {
             auto it = m_XMap.find(ports[i]);
             if (it == m_XMap.end())
             {
                 std::cout << "Error in get_dre_of_egress_ports() with invalid portId: " << ports[i] << std::endl;
                 PrintDreTable();
                 return dres;
             }
             else
             {
                 uint32_t dre = QuantizingX(it->second);
                 dres[i] = dre;
             }
         }
         return dres;
     }*/

    uint32_t RdmaSmartFlowRouting::forward_normal_packet(Ptr<Packet> &p, CustomHeader &ch, uint32_t srcHostId, uint32_t dstHostId, uint32_t pg)
    {
        // NS_LOG_INFO("PST strategy=" << m_pathSelStrategy << "<1:min, 2:rbnp, 3:rnd, 4:lspray, 5:flet, 6:ecmp, 7:rbnf, 8:rbnh>");
        // ############ Function: forward_normal_packet() ############");

        if (m_pathSelStrategy == PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY)
        {

            std::vector<PathData *> forwardPitEntries;
            HostId2PathSeleKey forwarPstKey(srcHostId, dstHostId);
            pstEntryData *forwardPstEntry = lookup_PST(forwarPstKey);
            NS_LOG_INFO("forward PST Entry");
            // forwardPstEntry->print();

            uint32_t forwardPathNum = forwardPstEntry->pathNum;
            NS_LOG_INFO("m_pathSelNum, forwardPathNum=" << m_pathSelNum << ", " << forwardPathNum);
            if (m_pathSelNum >= forwardPathNum)
            {
                NS_LOG_INFO("There is too Less available forwarding paths");
                forwardPitEntries = batch_lookup_PIT(forwardPstEntry->paths);
            }
            else
            {
                NS_LOG_INFO("There is too Many available forwarding paths, so to select " << m_pathSelNum << " paths");
                for (uint32_t i = 0; i < m_pathSelNum; i++)
                {
                    uint32_t rndPathIdx = std::rand() % forwardPathNum;
                    // std::cout << "The " << i << "-th path index is " << rndPathIdx << std::endl;
                    uint32_t rndPathId = forwardPstEntry->paths[rndPathIdx];
                    PathData *rndPitEntry = lookup_PIT(rndPathId);
                    forwardPitEntries.push_back(rndPitEntry);
                }
            }
            NS_LOG_INFO("forward PIT Entries");
            // for (auto e : forwardPitEntries) {
            //     e->print();
            // }

            forward_probe_packet_optimized(p, forwardPitEntries, ch, pg); // probing

            HostId2PathSeleKey reversePstKey(dstHostId, srcHostId);
            pstEntryData *reversePstEntry = lookup_PST(reversePstKey);
            NS_LOG_INFO("reverse PST Entry");
            // reversePstEntry->print();
            uint32_t reversePathNum = reversePstEntry->pathNum;
            NS_LOG_INFO("m_piggyLatencyCnt, reversePathNum=" << m_piggyLatencyCnt << ", " << reversePathNum);

            std::vector<PathData *> reversePitEntries;
            if (m_piggyLatencyCnt >= reversePathNum)
            {
                NS_LOG_INFO("There is too Less available reverse paths");
                reversePitEntries = batch_lookup_PIT(reversePstEntry->paths);
            }
            else
            {
                NS_LOG_INFO("There is too Many available reverse paths, so to select " << m_piggyLatencyCnt << " paths");
                for (uint32_t i = 0; i < m_piggyLatencyCnt; i++)
                {
                    uint32_t rndPathIdx = std::rand() % reversePathNum;
                    // std::cout << "The " << i << "-th path index is " << rndPathIdx << std::endl;
                    uint32_t rndPathId = reversePstEntry->paths[rndPathIdx];
                    PathData *rndPitEntry = lookup_PIT(rndPathId);
                    reversePitEntries.push_back(rndPitEntry);
                }
            }
            NS_LOG_INFO("reverse PIT Entries");
            // for (auto e : reversePitEntries) {
            //     e->print();
            // }
            std::vector<PathData *> feedBackPitEntries;
            if (m_enableFeedbackAllPathInfo)
            {
                feedBackPitEntries = reversePitEntries;
            }
            else
            {
                feedBackPitEntries = get_the_newly_measured_paths(reversePitEntries);
            }

            // std::vector<PathData *> freshrPitEntries = get_the_newly_measured_paths(reversePitEntries);
            NS_LOG_INFO("piggyback PIT Entries");
            // for (auto e : freshrPitEntries) {
            //     e->print();
            // }
            if (m_enabledAllPacketFeedbackInfo)
            {
                add_latency_tag_by_pit_entries(p, feedBackPitEntries);
                update_PIT_after_piggybacking(feedBackPitEntries); // piggybacking
            }
            else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) // ACK or NACK piggybacking
            {
                add_latency_tag_by_pit_entries(p, feedBackPitEntries);
                update_PIT_after_piggybacking(feedBackPitEntries);
            }

            NS_LOG_INFO("PIT Entries after piggybacking");
            // for (auto e : freshrPitEntries) {
            //     e->print();
            // }
            std::sort(forwardPitEntries.begin(), forwardPitEntries.end(), cmp_pitEntry_in_increase_order_of_latency);
            PathData *curBestForwardPitEntry = forwardPitEntries[0];
            NS_LOG_INFO("The cur Best Forward Pit Entry");
            // curBestForwardPitEntry->print();

            pdt_entry_t *bestForwadPitEntry = lookup_PDT(forwarPstKey);
            if (bestForwadPitEntry == 0)
            {
                // std::cout << "The new flow, to build a PDT entry" << std::endl;
                pdt_entry_t pdtEntry = pdt_entry_t();
                // pdtEntry.pid = curBestForwardPitEntry->pid;
                // pdtEntry.latency = curBestForwardPitEntry->latency;
                pdtEntry.pit = curBestForwardPitEntry;
                m_pathDecTbl[forwarPstKey] = pdtEntry;
                // m_pathDecTbl[forwarPstKey].print();
                bestForwadPitEntry = &m_pathDecTbl[forwarPstKey];
            }
            else if (curBestForwardPitEntry->latency < bestForwadPitEntry->pit->latency)
            {
                NS_LOG_INFO("The exist flow, the better Pid, to update pid and latency in PDT entry");
                // bestForwadPitEntry->print();
                bestForwadPitEntry->pit = curBestForwardPitEntry;
                // bestForwadPitEntry->latency = curBestForwardPitEntry->latency;
                // bestForwadPitEntry->print();
            }
            else
            {
                NS_LOG_INFO("The exist flow, the worse Pid, use the previous PDT entry");
            }
            uint32_t bestForwardPid = bestForwadPitEntry->pit->pid;
            NS_LOG_INFO("The final forward path Id");
            // std::cout << "bestForwardPid: "<< bestForwardPid << std::endl;
            add_path_tag_by_path_id(p, bestForwardPid);
            output_packet_by_path_tag(p, ch, pg); // sending
            return p->GetSize();
        }
        else
        {
            NS_LOG_INFO("m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY");
            return 0;
        }
    }

    uint32_t RdmaSmartFlowRouting::forward_probe_packet_optimized(Ptr<Packet> pkt, std::vector<PathData *> &forwardPitEntries, CustomHeader &ch, uint32_t pg)
    {

        NS_LOG_INFO("############ Fucntion: forward_probe_packet_optimized() ############");
        if (m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY)
        {
            NS_LOG_INFO("m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY");
            return 0;
        }

        std::vector<PathData *> expiredPitEntries, probePitEntries, potentialPitEntries;
        get_the_expired_paths(forwardPitEntries, expiredPitEntries);
        get_the_probe_paths(expiredPitEntries, probePitEntries);
        PathData *bestProbingPitEntry = get_the_best_probing_path(probePitEntries);
        if (bestProbingPitEntry == 0)
        {
            std::cout << "No need to probe" << std::endl;
            return 0;
        }
        std::cout << "The probe path" << std::endl;
        bestProbingPitEntry->print();

        Ptr<Packet> probePacket = construct_probe_packet(pkt, ch);
        std::cout << "probePacket" << std::endl;
        add_path_tag_by_path_id(probePacket, bestProbingPitEntry->pid);
        add_probe_tag_by_path_id(probePacket, bestProbingPitEntry->pid);
        CustomHeader probech(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
        if (probePacket->PeekHeader(probech) > 0)
        {
            NS_LOG_INFO("Extracted CustomHeader:");
            probePacket->AddPacketTag(FlowIdTag(1));
        }
        else
        {
            NS_LOG_WARN("Failed to extract CustomHeader from the packet.");
        }
        uint32_t probepg = 3;
        output_packet_by_path_tag(probePacket, probech, probepg);
        update_PIT_after_probing(bestProbingPitEntry);
        // bestProbingPitEntry->print();
        record_the_probing_info(bestProbingPitEntry->pid);
        return probePacket->GetSize();
    }

    // void RdmaSmartFlowRouting::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
    //  UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb) {
    void RdmaSmartFlowRouting::RouteInput(Ptr<Packet> p, CustomHeader ch)
    {
        NS_LOG_INFO("########## Node: " << m_switch_id << ", Time: " << Simulator::Now().GetMicroSeconds() << "us, Size:" << p->GetSize() << " ################## Function: RouteInput() #######################");
        // std::cout << "########## Node: "<< get_node_id() << ", Time: "<< Simulator::Now().GetMicroSeconds() << "us, PktSize:" << p->GetSize() << std::endl;
        if (ch.l3Prot != 0x11 || ch.l3Prot != 0xFD)
        {
            NS_LOG_INFO("ch.udp.seq:" << std::to_string(ch.udp.seq) << ",ACK/PFC or other control pkts. Sw("
                                      << m_switch_id << "),l3Prot:" << ch.l3Prot);
        }
        // assert(ch.l3Prot == 0x11 || ch.l3Prot == 0xFD && "Only supports UDP data packets or (N)ACK packets");
        Ipv4Address srcServerAddr = Ipv4Address(ch.sip);
        Ipv4Address dstServerAddr = Ipv4Address(ch.dip);
        NS_LOG_INFO("(srcServer, dstServer)=(" << srcServerAddr << ", " << dstServerAddr << ")");

        uint32_t srcToRId = lookup_SMT(srcServerAddr)->torId;
        uint32_t srcHostId = lookup_SMT(srcServerAddr)->hostId;
        uint32_t dstHostId = lookup_SMT(dstServerAddr)->hostId;
        uint32_t dstToRId = lookup_SMT(dstServerAddr)->torId;
        // Ipv4Address dstToRAddr = lookup_VMT(dstServerAddr)->torAddr;

        // NS_LOG_INFO ("Arriving Packet: size=" << p->GetSize());
        // NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev << &lcb << &ecb);
        // NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
        // Ipv4Address dstServerAddr = header.GetDestination();
        // Ipv4Address srcServerAddr = header.GetSource();
        // NS_LOG_INFO ("(srcServer, dstServer)=("<< srcServerAddr<<", "<<dstServerAddr<<")");

        // NS_LOG_INFO("Time " << Simulator::Now() << ", ");
        // NS_LOG_INFO("Node " << get_node_id() << ", ");
        // NS_LOG_INFO("srcServerAddr " << srcServerAddr << ", ");
        // NS_LOG_INFO("dstServerAddr " << dstServerAddr << ", ");

        // Ipv4Address srcToRAddr = lookup_VMT(srcServerAddr)->torAddr;
        // Ipv4Address dstToRAddr = lookup_VMT(dstServerAddr)->torAddr;
        //  NS_LOG_INFO ("(pktSize, srcServer, dstServer, srcToRAddr, dstToRAddr)=(" << p->GetSize() << ", "<< srcServerAddr << ", " << dstServerAddr << ", " << srcToRAddr <<", " << dstToRAddr << ")");
        //  NS_LOG_INFO("srcToRAddr " << srcToRAddr << ", ");
        //  NS_LOG_INFO("dstToRAddr " << dstToRAddr << ", ");
        //  NS_LOG_INFO("\n");
        Ptr<Packet> packet = ConstCast<Packet>(p);
        Ipv4SmartFlowPathTag pathTag;
        Ipv4SmartFlowProbeTag probeTag;

        // bool existPathTag = exist_path_tag(packet, pathTag);
        // NS_LOG_INFO("existPathTag " << existPathTag);
        int32_t pg = ch.udp.pg;
        if (exist_path_tag(packet, pathTag) == false)
        { // reaching the src ToR Switch

            NS_LOG_FUNCTION("Reaching the Source ToR switch");
            if (srcToRId == dstToRId)
            { // do normal routing (only one path)
                NS_LOG_INFO("Intra Switch : (srcToR, dstToR)=" << srcToRId << ", " << dstToRId << ")");
                HostId2PathSeleKey forwarPstKey(srcHostId, dstHostId);
                pstEntryData *forwardPstEntry = lookup_PST(forwarPstKey);
                NS_LOG_INFO("forward PST Entry");
                add_path_tag_by_path_id(packet, forwardPstEntry->paths[0]);

                output_packet_by_path_tag(packet, ch, pg);
                return;
            }
            else
            {
                forward_normal_packet(packet, ch, srcHostId, dstHostId, pg);
                // forward_probe_packet(packet, forwardPitEntries, bestForwardingPitEntry, header, ucb, dstServerAddr);
                NS_LOG_INFO("FINISH the RouteInput");
                return;
            }
        }

        else
        {
            PathData *pitEntry;
            if (reach_the_DstToR_last_hop_of_path_tag(pathTag, pitEntry) == true)
            { // dst ToR
                NS_LOG_FUNCTION("Reaching the Destination ToR switch");
                if (exist_probe_tag(packet, probeTag) == true)
                { // probe pkt
                    NS_LOG_FUNCTION("Is a Probe Packet");
                    receive_probe_packet(probeTag);
                    NS_LOG_INFO("FINISH the RouteInput, has probe tag");
                    return;
                }
                else
                { // normal pkt
                    // NS_LOG_FUNCTION ("Is a Normal Packet");
                    receive_normal_packet(packet, pathTag, pitEntry);
                    if (m_reorderFlag != 0)
                    {
                        // record_reorder_at_dst_tor(packet, ch);
                    }
                    output_packet_by_path_tag(packet, ch, pg);
                    NS_LOG_INFO("FINISH the RouteInput, normal packet, end");
                    return;
                }
            }
            else
            { // mid ToR
                // NS_LOG_FUNCTION ("Reaching the Intermediate switch");

                output_packet_by_path_tag(packet, ch, pg); // normal pkt
                NS_LOG_INFO("FINISH the RouteInput, normal packet, mid");

                return;
            }
        }
    }

    /* void RdmaSmartFlowRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
     {
         std::cout << "Node: " << m_ipv4->GetObject<Node>()->GetId()
                   << ", RdmaSmartFlowRouting table" << std::endl;

         for (std::map<uint32_t, PathData>::const_iterator it = m_nexthopSelTbl.begin(); it != m_nexthopSelTbl.end(); it++)
         {
             std::cout << "Path Id: " << it->first << std::endl;
             std::cout << "port sequence: ";
             uint32_t size = it->second.portSequence.size();
             for (uint32_t i = 0; i < size; i++)
             {
                 std::cout << " " << it->second.portSequence[i];
             }
             std::cout << "latency: " << it->second.latency << std::endl;
         }
         return;
     }*/

    void RdmaSmartFlowRouting::set_PST(std::map<HostId2PathSeleKey, pstEntryData> &pathSelTbl)
    {
        m_pathSelTbl.insert(pathSelTbl.begin(), pathSelTbl.end());
        // NS_LOG_INFO ("m_pathSelTbl size " << m_pathSelTbl.size());
        return;
    }

    std::map<HostId2PathSeleKey, pstEntryData> RdmaSmartFlowRouting::get_PST() const
    {
        return m_pathSelTbl;
    }

    void RdmaSmartFlowRouting::set_PIT(std::map<uint32_t, PathData> &nexthopSelTbl)
    {
        m_nexthopSelTbl.insert(nexthopSelTbl.begin(), nexthopSelTbl.end());
        // NS_LOG_INFO ("m_nexthopSelTbl size " << m_nexthopSelTbl.size());
        return;
    }

    std::map<uint32_t, PathData> RdmaSmartFlowRouting::get_PIT() const
    {
        return m_nexthopSelTbl;
    }

    void RdmaSmartFlowRouting::set_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &vmVtepMapTbl)
    {
        m_vmVtepMapTbl.insert(vmVtepMapTbl.begin(), vmVtepMapTbl.end());
        // NS_LOG_INFO ("m_vmVtepMapTbl size " << m_vmVtepMapTbl.size());
        return;
    }

    std::map<Ipv4Address, hostIp2SMT_entry_t> RdmaSmartFlowRouting::get_SMT() const
    {
        return m_vmVtepMapTbl;
    }

    void RdmaSmartFlowRouting::set_probing_interval(uint32_t probeTimeInterval)
    {
        m_probeTimeInterval = probeTimeInterval;
        return;
    }

    uint32_t RdmaSmartFlowRouting::get_probing_interval() const
    {
        return m_probeTimeInterval;
    }

    void RdmaSmartFlowRouting::set_path_expire_interval(uint32_t a)
    {
        m_pathExpiredTimeThld = a;
        return;
    }
    uint32_t RdmaSmartFlowRouting::get_path_expire_interval() const
    {
        return m_pathExpiredTimeThld;
    }

    void RdmaSmartFlowRouting::set_max_piggyback_path_number(uint32_t piggyLatencyCnt)
    {
        m_piggyLatencyCnt = piggyLatencyCnt;
        return;
    }

    uint32_t RdmaSmartFlowRouting::get_max_piggyback_path_number() const
    {
        return m_piggyLatencyCnt;
    }

    void RdmaSmartFlowRouting::record_the_probing_info(uint32_t pathId)
    {
        if (pathId >= m_prbInfoTable.size())
        {
            m_prbInfoTable.resize(pathId + 1);
            m_prbInfoTable[pathId].pathId = pathId;
            m_prbInfoTable[pathId].probeCnt = 0;
        }
        m_prbInfoTable[pathId].probeCnt = m_prbInfoTable[pathId].probeCnt + 1;

        // auto it = ;
        // it = m_vmVtepMapTbl.find(serverAddr);
        // probeInfoEntry prbEntry;
        // prbEntry.nodeId = nodeId;
        // prbEntry.pathId = pathId;
        // prbEntry.sendTime = Simulator::Now().GetNanoSeconds();
        // // std::cout<<"%%%%%%%%%%%%%%%%NodeId:"<<prbEntry.pathId<<"\t PathId:"<<prbEntry.pathId<<"\t TimeStamp:"<<prbEntry.sendTime<<std::endl;
        // m_prbInfoTable.push_back(prbEntry);
    }

}