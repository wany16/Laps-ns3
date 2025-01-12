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
    double RdmaSmartFlowRouting::laps_alpha = 4;
    std::uniform_real_distribution<double> RdmaSmartFlowRouting::rndGen(0.0, 1.0);
    std::default_random_engine RdmaSmartFlowRouting::generator(std::random_device{}());

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
        m_nodeId = switch_id;
    }
    void RdmaSmartFlowRouting::SetNode(Ptr<Node> node)
    {
        // m_server_node_id = server_node_id;
        // m_nodeId = server_node_id;
        m_node = node;
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
                                              MakeBooleanChecker())
                                .AddAttribute("enabledE2ELb",
                                              "lb is installed server",
                                              BooleanValue(false),
                                              MakeBooleanAccessor(&RdmaSmartFlowRouting::lb_isInstallSever),
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
        NS_LOG_FUNCTION(this);
        NS_ASSERT_MSG(packet != 0, "Packet is NULL");
        Ipv4SmartFlowPathTag pathTag;
        packet->PeekPacketTag(pathTag);
        uint32_t selectedPortId = get_egress_port_id_by_path_tag(pathTag);
        update_path_tag(packet, pathTag);
        DoSwitchSend(packet, ch, selectedPortId, pg);
        return true;
    }
    bool RdmaSmartFlowRouting::e2eLBSrc_output_packet(Ptr<E2ESrcOutPackets> &srcOutEntry)
    {
        NS_LOG_INFO("############ Fucntion: e2eLBSrc_output_packet ############");
        // std::cout << "Enter output_packet_by_path_tag()"<<std::endl;
        if (srcOutEntry == nullptr)
        {
            NS_LOG_INFO("srcOutEntry==nullptr");
            return false;
        }
        Ipv4SmartFlowPathTag pathTag;
        srcOutEntry->dataPacket->PeekPacketTag(pathTag);
        // uint32_t maxQLen = pathTag.GetDre();
        // uint32_t selectedPortId = get_egress_port_id_by_path_tag(pathTag);
        update_path_tag(srcOutEntry->dataPacket, pathTag);

        if (srcOutEntry->isProbe)
        {
            Ipv4SmartFlowPathTag probePathTag;
            srcOutEntry->probePacket->PeekPacketTag(probePathTag);
            update_path_tag(srcOutEntry->probePacket, probePathTag);
        }
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
        NS_LOG_FUNCTION(this << pitEntries.size() << m_piggyLatencyCnt << pitEntries.size());
        NS_ASSERT_MSG(packet != 0 && m_piggyLatencyCnt != 0, "Packet or m_piggyLatencyCnt is NULL");
        uint32_t pathNum = pitEntries.size();
        if (pathNum == 0)
        {
            NS_LOG_INFO("No need to add latency tag since pathNum=" << pathNum << ", m_piggyCnt=" << m_piggyLatencyCnt);
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

    void RdmaSmartFlowRouting::update_PIT_after_adding_path_tag(PathData *pitEntry)
    {
        NS_LOG_FUNCTION(this);
        NS_ASSERT_MSG(pitEntry != 0, "PiggyBackPitEntry is NULL");
        pitEntry->tsProbeLastSend = Simulator::Now();
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
        NS_LOG_FUNCTION(this << piggyBackPitEntries.size());
        uint32_t pathNum = piggyBackPitEntries.size();
        for (uint32_t i = 0; i < pathNum; i++)
        {
            NS_ASSERT_MSG(piggyBackPitEntries[i] != 0, "PiggyBackPitEntry is NULL");
            piggyBackPitEntries[i]->tsLatencyLastSend = Simulator::Now();
        }
        return pathNum;
    }

    Ipv4SmartFlowPathTag RdmaSmartFlowRouting::construct_path_tag(uint32_t selectedPathId)
    {
        NS_LOG_FUNCTION(this << selectedPathId);
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
        NS_LOG_FUNCTION(this << pitEntries.size());
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
        NS_LOG_FUNCTION(this << allPitEntries.size());
        auto maxElement = std::max_element(allPitEntries.begin(), allPitEntries.end(),
                [](const PathData* lhs, const PathData* rhs) 
                {
                    return lhs->latency < rhs->latency;
                }
            );
        uint64_t expiredPeriodInNs = 2 * (*maxElement)->latency;
        expiredPitEntries.clear();
        uint64_t curTime = Simulator::Now().GetNanoSeconds();
        for (auto e : allPitEntries)
        {
            uint64_t Gentime = e->tsGeneration.GetNanoSeconds();
            NS_ASSERT_MSG(curTime >= Gentime, "GenTime cannot be larger than curtime");
            uint64_t timeGap = curTime - Gentime;
            if (timeGap > expiredPeriodInNs)
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
        NS_LOG_FUNCTION(this << expiredPitEntries.size());

        probePitEntries.clear();
        if (expiredPitEntries.size() == 0)
        {
            return 0;
        }

        auto maxElement = std::max_element(expiredPitEntries.begin(), expiredPitEntries.end(),
                [](const PathData* lhs, const PathData* rhs) 
                {
                    return lhs->latency < rhs->latency;
                }
            );
        uint64_t prbColdenPeriodInNs = 2 * (*maxElement)->latency;


        uint64_t curTime = Simulator::Now().GetNanoSeconds();
        for (auto e : expiredPitEntries)
        {
            uint64_t prbTime = e->tsProbeLastSend.GetNanoSeconds();
            NS_ASSERT_MSG(curTime >= prbTime, "ProbeTime cannot be larger than curtime");
            uint64_t timeGap = curTime - prbTime;
            if (timeGap > prbColdenPeriodInNs)
            {
                probePitEntries.push_back(e);
            }
        }
        return probePitEntries.size();
    }

    PathData *RdmaSmartFlowRouting::get_the_best_probing_path(std::vector<PathData *> &pitEntries)
    {
        NS_LOG_FUNCTION(this << pitEntries.size());
        // NS_LOG_INFO ("############ Fucntion: get_the_best_probing_path() ############");
        if (pitEntries.size() == 0)
        {
            return 0;
        }
        // if (m_probeStrategy == PROBE_SMALL_LATENCY_FIRST_STRATEGY)
        // {
        //     return get_the_smallest_latency_path(pitEntries);
        // }
        // else if (m_probeStrategy == PROBE_SMALL_GENERATION_TIME_FIRST_STRATEGY)
        // {
        //     return get_the_oldest_measured_path(pitEntries);
        // }
        // else if (m_probeStrategy == PROBE_RANDOM_STRATEGY)
        // { // hashflow
        //     return get_the_random_path(pitEntries);
        // }
        return get_the_random_path(pitEntries);
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
        NS_LOG_FUNCTION(this);
        uint32_t pid = latencyData.latencyInfo.first;
        uint32_t newLatency = latencyData.latencyInfo.second;
        Time newLatencyGeneration = latencyData.tsGeneration;
        PathData *pitEntry = lookup_PIT(pid);
        // uint32_t oldLatency = pitEntry->latency;
        Time oldLatencyGeneration = pitEntry->tsGeneration;
        if (newLatencyGeneration > oldLatencyGeneration)
        {
            NS_LOG_INFO ("Old Info: " << pitEntry->toString());
            pitEntry->latency = newLatency;
            pitEntry->tsGeneration = newLatencyGeneration;
            NS_LOG_INFO ("New Info: " << pitEntry->toString());
        }
    }

    void RdmaSmartFlowRouting::update_PIT_by_latency_tag(Ptr<Packet> &packet)
    {
        NS_LOG_FUNCTION(this << m_piggyLatencyCnt);
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
        NS_LOG_FUNCTION(this);
        Time curTime = Simulator::Now();
        Time oldTime = pathTag.GetTimeStamp();
        uint64_t newLatency = (uint64_t)(curTime.GetNanoSeconds() - oldTime.GetNanoSeconds());
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
        NS_LOG_FUNCTION(this);
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
        update_PIT_by_probe_tag(probeTag);
        return;
    }
    Ptr<Packet> RdmaSmartFlowRouting::reply_probe_info(Ptr<Packet> &p, CustomHeader &ch)
    {
        Ipv4SmartFlowProbeTag probeTag;
        p->PeekPacketTag(probeTag);
        Ptr<Packet> replyPacket = construct_reply_probe_packet(p, ch);
        uint32_t pid = probeTag.GetPathId();
        PathData *pitEntry = lookup_PIT(pid);
        std::vector<PathData *> feedBackPitEntries;
        feedBackPitEntries.push_back(pitEntry);
        add_latency_tag_by_pit_entries(p, feedBackPitEntries);
        update_PIT_after_piggybacking(feedBackPitEntries);
        return replyPacket;
    }

    void RdmaSmartFlowRouting::update_path_tag(Ptr<Packet> &packet, Ipv4SmartFlowPathTag &pathTag)
    {
        NS_LOG_FUNCTION(this);
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
    Ptr<Packet> RdmaSmartFlowRouting::construct_reply_probe_packet(Ptr<Packet> &pkt, CustomHeader &ch)
    {
        Ptr<Packet> probePacket = Create<Packet>(PROBE_DEFAULT_PKT_SIZE_IN_BYTE);
        // 添加 SeqTsHeader
        SeqTsHeader seqTs;
        seqTs.SetSeq(0); // 设置初始序列号
        probePacket->AddHeader(seqTs);
        // 添加 UdpHeader
        UdpHeader udpHeader;
        udpHeader.SetDestinationPort(ch.udp.dport); //
        udpHeader.SetSourcePort(ch.udp.sport);      //
        probePacket->AddHeader(udpHeader);

        Ipv4Header ipHeader;
        ipHeader.SetSource(Ipv4Address(ch.dip));
        ipHeader.SetDestination(Ipv4Address(ch.sip));
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
        NS_LOG_FUNCTION(this << pstKey.to_string());
        std::map<HostId2PathSeleKey, pstEntryData>::iterator it = m_pathSelTbl.find(pstKey);
        NS_ASSERT_MSG(it != m_pathSelTbl.end(), "Cannot match any entry in PST for the Key " << pstKey.to_string());
        return &(it->second);
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
        NS_LOG_FUNCTION(this);
        std::map<uint32_t, PathData>::iterator it = m_nexthopSelTbl.find(pieKey);
        NS_ASSERT_MSG(it != m_nexthopSelTbl.end(), "Cannot match any entry in PIT for the Key " << pieKey);
        return &(it->second);
    }
    std::vector<PathData *> RdmaSmartFlowRouting::batch_lookup_PIT(std::vector<uint32_t> &pids)
    {
        NS_LOG_FUNCTION(this <<  vectorTostring(pids));
        std::vector<PathData *> pitEntries(pids.size());
        for (uint32_t i = 0; i < pitEntries.size(); i++)
        {
            pitEntries[i] = lookup_PIT(pids[i]);
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
        NS_LOG_FUNCTION(this);
        update_PIT_by_path_tag(pathTag, pitEntry);
        update_PIT_by_latency_tag(pkt);
        return;
    }

    void RdmaSmartFlowRouting::add_probe_tag_by_path_id(Ptr<Packet> packet, uint32_t expiredPathId)
    {
        NS_LOG_FUNCTION(this << expiredPathId);
        Ipv4SmartFlowProbeTag probeTag = construct_probe_tag_by_path_id(expiredPathId);
        packet->AddPacketTag(probeTag);
        return;
    }

    void RdmaSmartFlowRouting::update_PIT_after_probing(PathData *pitEntry)
    {
        NS_LOG_FUNCTION(this);
        pitEntry->tsProbeLastSend = Simulator::Now();
        return;
    }

    void RdmaSmartFlowRouting::add_path_tag_by_path_id(Ptr<Packet> packet, uint32_t pid)
    {
        NS_LOG_FUNCTION(this << pid);
        Ipv4SmartFlowPathTag pathTag = construct_path_tag(pid);
        packet->AddPacketTag(pathTag);
        return;
    }

    uint32_t RdmaSmartFlowRouting::get_the_path_length_by_path_id(const uint32_t pathId, PathData *&pitEntry)
    {
        // NS_LOG_INFO ("############ Fucntion: get_the_path_length_by_path_id() ############");
        PathData *pitEntry = lookup_PIT(pathId);
        return pitEntry->portSequence.size();
    }

    bool RdmaSmartFlowRouting::reach_the_last_hop_of_path_tag(Ipv4SmartFlowPathTag &smartFlowTag, PathData *&pitEntry)
    {
        NS_LOG_FUNCTION(this);
        uint32_t pathId = smartFlowTag.get_path_id();
        uint32_t pathSize = get_the_path_length_by_path_id(pathId, pitEntry);
        uint32_t hopIdx = smartFlowTag.get_the_current_hop_index();
        NS_ASSERT_MSG(hopIdx <= pathSize, "The hopIdx is larger than the pathSize");
        if (hopIdx == pathSize)
        {
            NS_LOG_INFO("The packet reaches the last hop of Path " << pathId);
            return true;
        }
        else
        {
            // NS_LOG_INFO("Is " << hopIdx << "/" << pathSize << " hops");
            return false;
        }
    }

    bool RdmaSmartFlowRouting::exist_path_tag(Ptr<Packet> packet, Ipv4SmartFlowPathTag &pathTag)
    {
        NS_LOG_FUNCTION(this);
        if (packet->PeekPacketTag(pathTag))
        { // reaching the src ToR switch
            // NS_LOG_INFO("Path Tag indeed exits");
            uint32_t curHopIdx = pathTag.get_the_current_hop_index();
            uint32_t pathId = pathTag.get_path_id();
            NS_LOG_INFO("Packet " << packet->GetUid() << " is on the " << curHopIdx << "-th hop of Path " << pathId);
            return true;
        }
        // NS_LOG_INFO("Path Tag does not exit");
        return false;
    }

    bool RdmaSmartFlowRouting::exist_ack_tag(Ptr<Packet> packet, AckPathTag &ackTag)
    {
        NS_LOG_FUNCTION(this);
        if (packet->PeekPacketTag(ackTag))
        {
            NS_LOG_INFO("ACK Tag : pktID=" << packet->GetUid() << ",  flowID=" << ackTag.GetFlowId() << ", pathID= " << ackTag.GetPathId());
            return true;
        }
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
        NS_ASSERT_MSG(pitEntry != 0 && hopIdx < pitEntry->portSequence.size(), "The hopIdx is larger than the pathSize");
        return pitEntry->portSequence[hopIdx];
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

    uint32_t RdmaSmartFlowRouting::forward_normal_packet(Ptr<Packet> &p, CustomHeader &ch, uint32_t srcHostId, uint32_t dstHostId, uint32_t pg, Ptr<E2ESrcOutPackets> &srcOutEntry)
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
                rndPathIdStartValue = std::rand() % (forwardPathNum - m_pathSelNum + 1);
                for (uint32_t i = 0; i < m_pathSelNum; i++)
                {
                    // uint32_t rndPathIdx = std::rand() % forwardPathNum;
                    //  std::cout << "The " << i << "-th path index is " << rndPathIdx << std::endl;
                    uint32_t rndPathId = forwardPstEntry->paths[rndPathIdStartValue];
                    PathData *rndPitEntry = lookup_PIT(rndPathId);
                    forwardPitEntries.push_back(rndPitEntry);
                    rndPathIdStartValue++;
                }
            }
            NS_LOG_INFO("forward PIT Entries");
            // for (auto e : forwardPitEntries) {
            //     e->print();
            // }

            forward_probe_packet_optimized(p, forwardPitEntries, ch, pg, srcOutEntry); // probing

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
            NS_LOG_INFO("The final forward path Id " << bestForwardPid);
            // std::cout << "bestForwardPid: "<< bestForwardPid << std::endl;
            add_path_tag_by_path_id(p, bestForwardPid);
            if (IsE2ELb())
            {
                srcOutEntry->dataPacket = p;
                e2eLBSrc_output_packet(srcOutEntry);
            }
            else
            {
                output_packet_by_path_tag(p, ch, pg); // sending
            }
            return p->GetSize();
        }
        else
        {
            NS_LOG_INFO("m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY");
            return 0;
        }
    }

    uint32_t RdmaSmartFlowRouting::forward_probe_packet_optimized(Ptr<Packet> pkt, std::vector<PathData *> &forwardPitEntries, CustomHeader &ch, uint32_t pg, Ptr<E2ESrcOutPackets> &srcOutEntry)
    {
        FlowIdTag flowIdTag;
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
            if (!IsE2ELb())
            {
                // probePacket->AddPacketTag(FlowIdTag(1));
            }

            if (!probePacket->PeekPacketTag(flowIdTag))
            {
                // 如果标签不存在，则添加
                flowIdTag.SetFlowId(1);
                probePacket->AddPacketTag(flowIdTag);
            }
            else
            {
                NS_LOG_INFO("probePacket FlowIdTag already exists on packet " << probePacket->GetUid());
            }

            // probePacket->AddPacketTag(FlowIdTag(1));
        }
        else
        {
            NS_LOG_WARN("Failed to extract CustomHeader from the packet.");
        }
        uint32_t probepg = 3;
        if (IsE2ELb())
        {
            srcOutEntry->probePacket = probePacket;
            srcOutEntry->isProbe = true;
        }
        else
        {
            output_packet_by_path_tag(probePacket, probech, probepg);
        }
        update_PIT_after_probing(bestProbingPitEntry);

        // bestProbingPitEntry->print();
        record_the_probing_info(bestProbingPitEntry->pid);
        return probePacket->GetSize();
    }

    // void RdmaSmartFlowRouting::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
    //  UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb) {
    bool RdmaSmartFlowRouting::RouteInput(Ptr<Packet> p, CustomHeader ch)
    {
        NS_LOG_FUNCTION(this << m_node->GetId()
                             << p->GetSize()
                             << Simulator::Now().GetMicroSeconds()
                        );
        NS_LOG_INFO("#Node: " << m_node->GetId() << ", "
                    <<"Time: " << Simulator::Now().GetNanoSeconds() << " ns, "
                    << "Size: " << p->GetSize() << " Bytes, " 
                    << "Receive PktID:" << p->GetUid() << " "
                    );
        Ipv4SmartFlowPathTag pathTag;
        NS_ASSERT_MSG(exist_path_tag(p, pathTag), "The path tag does not exist");
        bool ShouldUpForward = true;

        PathData *pitEntry;
        bool isReachDst = reach_the_last_hop_of_path_tag(pathTag, pitEntry);
        if (isReachDst)
        {   
            Ipv4SmartFlowProbeTag probeTag;
            if (exist_probe_tag(p, probeTag))
            {   
                receive_probe_packet(probeTag);
                ShouldUpForward = false;
            }
            else
            {   
                receive_normal_packet(p, pathTag, pitEntry);
            }
        }
        else
        { 
            output_packet_by_path_tag(p, ch, ch.udp.pg);
            ShouldUpForward = false;
        }
        return ShouldUpForward;
    }

// 计算Softmax函数
std::vector<double> RdmaSmartFlowRouting::CalPathWeightBasedOnDelay(const std::vector<PathData *> paths) {
    NS_LOG_FUNCTION(this << paths.size());
    auto maxElement = std::max_element(paths.begin(), paths.end(),
            [](const PathData* lhs, const PathData* rhs) 
            {
                return lhs->theoreticalSmallestLatencyInNs < rhs->theoreticalSmallestLatencyInNs;
            }
    );
    uint64_t maxBastDelay = (*maxElement)->theoreticalSmallestLatencyInNs;
    NS_LOG_INFO("The targetDelay is " << maxBastDelay << " ns");
    std::vector<double> weights(paths.size());
    double sum_weights = 0.0;
    for (size_t i = 0; i < weights.size(); i++)
    {
        double ratio = -1.0 * paths[i]->latency/maxBastDelay  * laps_alpha;
        weights[i] = std::exp(ratio);
        sum_weights += weights[i];
    }
    NS_ASSERT_MSG(sum_weights > 0.0, "The sum_weights is zero");
    for (size_t i = 0; i < weights.size(); i++)
    {
        weights[i] /= sum_weights;
        NS_LOG_INFO("Path: " << paths[i]->pid << ", " <<
                    "Realtime Delay: " << paths[i]->latency << ", " <<
                    "Mininal Delay: " << paths[i]->theoreticalSmallestLatencyInNs << ", " <<
                    "Weight: " << weights[i]
                   );
    }

    return weights;
}

uint32_t RdmaSmartFlowRouting::GetPathBasedOnWeight(const std::vector<double> & weights) 
{
    const double random_value = rndGen(generator);
    NS_ASSERT_MSG(random_value >= 0.0 && random_value <= 1.0, "Wrong random value");
    NS_ASSERT_MSG(weights.size() > 0, "The weights is empty");
    double cumulative_weight = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) 
    {
        cumulative_weight += weights[i];
        if (random_value < cumulative_weight) 
        {
            return i;
        }
    }
    return weights.size() - 1;
}


    void RdmaSmartFlowRouting::RouteOutput(Ptr<Packet> p, CustomHeader ch, Ptr<E2ESrcOutPackets> &srcOutEntry)
    {
        NS_LOG_FUNCTION(this << m_node->GetId()
                             << p->GetSize()
                             << Ipv4Address(ch.sip)
                             << Ipv4Address(ch.dip)
                             << Simulator::Now().GetMicroSeconds()
                        );

        Ipv4Address srcServerAddr = Ipv4Address(ch.sip);
        Ipv4Address dstServerAddr = Ipv4Address(ch.dip);
        uint32_t srcHostId = lookup_SMT(srcServerAddr)->hostId;
        uint32_t dstHostId = lookup_SMT(dstServerAddr)->hostId;

        NS_ASSERT_MSG(p != 0 && srcOutEntry != 0, "The packet or srcOutEntry is null");

        // forwarding
        srcOutEntry->dataPacket = ConstCast<Packet>(p);
        Ipv4SmartFlowPathTag pathTag;
        NS_ASSERT_MSG(!exist_path_tag(srcOutEntry->dataPacket, pathTag), "Should not have path tag");
        HostId2PathSeleKey pstKey(srcHostId, dstHostId);
        pstEntryData *pstEntry = lookup_PST(pstKey);
        std::vector<PathData *> pitEntries = batch_lookup_PIT(pstEntry->paths);
        std::vector<double> weights = CalPathWeightBasedOnDelay(pitEntries);
        uint32_t selPathIndex = GetPathBasedOnWeight(weights);
        NS_ASSERT_MSG(selPathIndex < pitEntries.size(), "The selected path index is out of range");
        uint32_t fPid = pitEntries[selPathIndex]->pid;
        add_path_tag_by_path_id(srcOutEntry->dataPacket, fPid);
        update_PIT_after_adding_path_tag(pitEntries[selPathIndex]);

        // piggybacking if ack
        if (srcOutEntry->isAck)
        {
            HostId2PathSeleKey reversePstKey(dstHostId, srcHostId);
            pstEntryData *reversePstEntry = lookup_PST(reversePstKey);
            NS_ASSERT_MSG(reversePstEntry != 0, "The reversePstEntry is null");
            std::vector<PathData *> reversePitEntries = batch_lookup_PIT(reversePstEntry->paths);
            add_latency_tag_by_pit_entries(srcOutEntry->dataPacket, reversePitEntries);
            update_PIT_after_piggybacking(reversePitEntries); // piggybacking
        }

        // probing
        std::vector<PathData *> expiredPitEntries, probePitEntries;
        get_the_expired_paths(pitEntries, expiredPitEntries);
        get_the_probe_paths(expiredPitEntries, probePitEntries);
        PathData *probePitEntry = get_the_best_probing_path(probePitEntries);
        NS_ASSERT_MSG((probePitEntries.size() != 0 && probePitEntry != 0 && fPid == probePitEntry->pid) || (probePitEntries.size() == 0 && probePitEntry == 0), "The probePitEntry is null");
        if (probePitEntry == 0)
        {
            srcOutEntry->probePacket = 0;
            srcOutEntry->isProbe = false;
        }
        else
        {
            srcOutEntry->isProbe = true;
            srcOutEntry->probePacket = construct_probe_packet(srcOutEntry->dataPacket, ch);
            add_path_tag_by_path_id(srcOutEntry->probePacket, probePitEntry->pid);
            add_probe_tag_by_path_id(srcOutEntry->probePacket, probePitEntry->pid);
            update_PIT_after_probing(probePitEntry);
            record_the_probing_info(probePitEntry->pid);
        }

        return;
    }



    void RdmaSmartFlowRouting::RouteOutputForDataPktOnSrcHostForLaps(Ptr<E2ESrcOutPackets> entry)
    {
        NS_LOG_FUNCTION(this << "Node: " << m_nodeId);
		NS_ASSERT_MSG(entry && entry->isData && !entry->isAck, "invalid entry");

		Ptr<Packet> p = entry->dataPacket;
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		NS_ASSERT_MSG(p->PeekHeader(ch) > 0, "Failed to extract CustomHeader");
        Ipv4SmartFlowPathTag pathTag;
        NS_ASSERT_MSG(!exist_path_tag(p, pathTag), "Should not have path tag");

        // forwarding
        Ipv4Address srcServerAddr = Ipv4Address(ch.sip);
        Ipv4Address dstServerAddr = Ipv4Address(ch.dip);
        uint32_t srcHostId = lookup_SMT(srcServerAddr)->hostId;
        uint32_t dstHostId = lookup_SMT(dstServerAddr)->hostId;
        HostId2PathSeleKey pstKey(srcHostId, dstHostId);
        pstEntryData *pstEntry = lookup_PST(pstKey);
        std::vector<PathData *> pitEntries = batch_lookup_PIT(pstEntry->paths);
        std::vector<double> weights = CalPathWeightBasedOnDelay(pitEntries);
        uint32_t selPathIndex = GetPathBasedOnWeight(weights);
        NS_ASSERT_MSG(selPathIndex < pitEntries.size(), "The selected path index is out of range");
        uint32_t fPid = pitEntries[selPathIndex]->pid;
        add_path_tag_by_path_id(entry->dataPacket, fPid);
        NS_ASSERT_MSG(entry->lastQp, "The lastQp is null");
        if (Irn::mode == Irn::Mode::NACK)
        {
            uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();
            // entry->lastQp->m_irn.m_sack.appendOutstandingData(fPid, ch.udp.seq, payload_size);
			// NS_LOG_INFO("PktId: " << p->GetUid()  << " Type: DATA, Size: " << payload_size <<	" Pid: " << fPid);
            entry->latencyForDataPktInNs = pitEntries[selPathIndex]->latency;
            entry->pidForDataPkt = fPid;
        }
        update_PIT_after_adding_path_tag(pitEntries[selPathIndex]);


        // probing
        PathData *prbeEntry = CheckProbePathAmoungPitEntries(pitEntries);
        if (prbeEntry != 0)
        {
            entry->isProbe = true;
            entry->probePacket = construct_probe_packet(entry->dataPacket, ch);
            add_path_tag_by_path_id(entry->probePacket, prbeEntry->pid);
            add_probe_tag_by_path_id(entry->probePacket, prbeEntry->pid);
			// NS_LOG_INFO("PktId: " << entry->probePacket->GetUid()  << " Type: Probe, Size: " << entry->probePacket->GetSize()-ch.GetSerializedSize() <<	" Pid: " << prbeEntry->pid);
            update_PIT_after_probing(prbeEntry);
            record_the_probing_info(prbeEntry->pid);
        }else{
            entry->isProbe = false;
            entry->probePacket = NULL;
        }

        return;
    }

    void RdmaSmartFlowRouting::RouteOutputForAckPktOnSrcHostForLaps(Ptr<E2ESrcOutPackets> entry)
    {
        NS_LOG_FUNCTION(this << "Node: " << m_nodeId);
		NS_ASSERT_MSG(entry && !entry->isData && entry->isAck, "invalid entry");
        


		Ptr<Packet> p = entry->ackPacket;
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		NS_ASSERT_MSG(p->PeekHeader(ch) > 0, "Failed to extract CustomHeader");
        Ipv4SmartFlowPathTag pathTag;
        NS_ASSERT_MSG(!exist_path_tag(p, pathTag), "Should not have path tag");
        AckPathTag ackTag;
        NS_ASSERT_MSG((Irn::mode == Irn::Mode::NACK) ^ (!exist_ack_tag(p, ackTag)), "Should not have ack tag xor nack tag");
        // forwarding
        Ipv4Address srcServerAddr = Ipv4Address(ch.sip);
        Ipv4Address dstServerAddr = Ipv4Address(ch.dip);
        uint32_t srcHostId = lookup_SMT(srcServerAddr)->hostId;
        uint32_t dstHostId = lookup_SMT(dstServerAddr)->hostId;
        HostId2PathSeleKey pstKey(srcHostId, dstHostId);
        pstEntryData *pstEntry = lookup_PST(pstKey);
        std::vector<PathData *> pitEntries = batch_lookup_PIT(pstEntry->paths);
        NS_ASSERT_MSG(pitEntries.size() > 0, "The pitEntries is empty");
        uint32_t selPathIndex = 0;
        if (Irn::mode == Irn::Mode::NACK)
        {
            std::vector<uint32_t> key = {ackTag.GetPathId(), ackTag.GetFlowId()};
            size_t val = GetHashValue<uint32_t>(key);
            selPathIndex = val % pitEntries.size();
        }
        else if (Irn::mode == Irn::Mode::IRN_OPT)
        {
            std::vector<double> weights = CalPathWeightBasedOnDelay(pitEntries);
            selPathIndex = GetPathBasedOnWeight(weights);
        }
        else
        {
            NS_ASSERT_MSG(false, "The mode is invalid");
        }
        NS_ASSERT_MSG(selPathIndex < pitEntries.size(), "The selected path index is out of range");
        uint32_t fPid = pitEntries[selPathIndex]->pid; 
        add_path_tag_by_path_id(entry->ackPacket, fPid);
	    // NS_LOG_INFO("PktId: " << p->GetUid()  << " Type: ACK, Size: " << p->GetSize()-ch.GetSerializedSize() <<	" fPid: " << fPid << " rPid: " << ackTag.GetPathId());

        // probing
        PathData *prbeEntry = CheckProbePathAmoungPitEntries(pitEntries);
        if (prbeEntry != 0)
        {
            entry->isProbe = true;
            entry->probePacket = construct_probe_packet(entry->ackPacket, ch);
            add_path_tag_by_path_id(entry->probePacket, prbeEntry->pid);
            add_probe_tag_by_path_id(entry->probePacket, prbeEntry->pid);
			// NS_LOG_INFO("PktId: " << entry->probePacket->GetUid()  << " Type: Probe, Size: " << entry->probePacket->GetSize()-ch.GetSerializedSize() <<	" Pid: " << prbeEntry->pid);
            update_PIT_after_probing(prbeEntry);
            record_the_probing_info(prbeEntry->pid);
        }else{
            entry->isProbe = false;
            entry->probePacket = NULL;
        }

        // piggybacking
        HostId2PathSeleKey reversePstKey(dstHostId, srcHostId);
        pstEntryData *reversePstEntry = lookup_PST(reversePstKey);
        NS_ASSERT_MSG(reversePstEntry != 0, "The reversePstEntry is null");
        std::vector<PathData *> reversePitEntries = batch_lookup_PIT(reversePstEntry->paths);
        add_latency_tag_by_pit_entries(p, reversePitEntries);
        update_PIT_after_piggybacking(reversePitEntries);

        return;
    }

    PathData * RdmaSmartFlowRouting::CheckProbePathAmoungPitEntries(std::vector<PathData *> & pitEntries)
    {
        NS_LOG_FUNCTION(this << m_nodeId);
        std::vector<PathData *> expiredPitEntries, probePitEntries;
        get_the_expired_paths(pitEntries, expiredPitEntries);
        get_the_probe_paths(expiredPitEntries, probePitEntries);
        PathData *probePitEntry = get_the_best_probing_path(probePitEntries);
        NS_ASSERT_MSG((probePitEntries.size() != 0 && probePitEntry != 0) || (probePitEntries.size() == 0 && probePitEntry == 0), "The probePitEntry is null");
        return probePitEntry;
    }



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

    bool RdmaSmartFlowRouting::insert_entry_to_PIT(PathData &pitEntry)
    {
        uint32_t pid = pitEntry.pid;
        auto it = m_nexthopSelTbl.find(pid);
        if (it == m_nexthopSelTbl.end())
        {
            m_nexthopSelTbl[pid] = pitEntry;
            return true;
        }
        else
        {
            NS_ASSERT_MSG(false, "The entry already exists in PIT");
            return false;
        }

    }

    bool RdmaSmartFlowRouting::insert_entry_to_SMT(hostIp2SMT_entry_t &smtEntry)
    {
        Ipv4Address key = smtEntry.hostIp;
        auto it = m_vmVtepMapTbl.find(key);
        if (it == m_vmVtepMapTbl.end())
        {
            m_vmVtepMapTbl[key] = smtEntry;
            return true;
        }
        else
        {
            NS_ASSERT_MSG(false, "The entry already exists in SMT");
            return false;
        }
    }


    bool RdmaSmartFlowRouting::insert_entry_to_PST(pstEntryData &pstEntry)
    {
        HostId2PathSeleKey pstKey = pstEntry.key;
        auto it = m_pathSelTbl.find(pstKey);
        if (it == m_pathSelTbl.end())
        {
            m_pathSelTbl[pstKey] = pstEntry;
            return true;
        }
        else
        {
            NS_ASSERT_MSG(false, "The entry already exists in PST");
            return false;
        }
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
    bool RdmaSmartFlowRouting::IsE2ELb(void)
    {
        return lb_isInstallSever;
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