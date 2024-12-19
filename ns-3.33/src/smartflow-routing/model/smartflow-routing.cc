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

#include "smartflow-routing.h"


namespace ns3 {

// 在类外部初始化静态成员变量  
std::vector<probeInfoEntry> Ipv4SmartFlowRouting::m_prbInfoTable(0);
std::map <std::string, reorder_entry_t> Ipv4SmartFlowRouting::m_reorderTable;
std::vector<uint64_t> Ipv4SmartFlowRouting::m_pathhitTable(0);

void Ipv4SmartFlowRouting::printPathhit(std::string file){
  std::ofstream os(file.c_str());
  if (!os.is_open()){
    std::cout << "printPathhit() cannot open file " << file << std::endl;
  }
  uint32_t idx = 0;
  for (uint32_t i =0; i < m_pathhitTable.size(); i++) {
    os << i << ", ";
    os << m_pathhitTable[i];    
    os << std::endl;
  }
  os.close();
}

NS_LOG_COMPONENT_DEFINE ("Ipv4SmartFlowRouting");

NS_OBJECT_ENSURE_REGISTERED (Ipv4SmartFlowRouting);

Ipv4SmartFlowRouting::Ipv4SmartFlowRouting () : m_ipv4(0),
    m_alpha(0.2),
      // Quantizing bits
    m_Q(3),
    m_C(DataRate("10Gbps")),
    m_agingTime (MilliSeconds (10)),
    m_tdre(MicroSeconds(100)),
    m_probeTimeInterval(PROBE_DEFAULT_INTERVAL_IN_NANOSECOND),
    m_piggyLatencyCnt(0) {
    NS_LOG_FUNCTION (this);
    m_pathSelTbl.clear();
    m_nexthopSelTbl.clear(); 
    m_vmVtepMapTbl.clear();
}
Ipv4SmartFlowRouting::~Ipv4SmartFlowRouting () {
    NS_LOG_FUNCTION (this);
}

uint32_t Ipv4SmartFlowRouting::get_node_id (void) const {
    uint32_t dstId = m_ipv4->GetObject<Node> ()->GetId ();
    return dstId;
}   

TypeId Ipv4SmartFlowRouting::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::Ipv4SmartFlowRouting")
      .SetParent<Object> ()
      .SetGroupName ("Internet")
      .AddConstructor<Ipv4SmartFlowRouting> ()
      .AddAttribute ("probeTimeInterval", " Probe can be send only exceeds this time interval",
                   UintegerValue (PROBE_DEFAULT_INTERVAL_IN_NANOSECOND),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_probeTimeInterval),
                   MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("pathExpiredTimeThld", " Probe can be send only exceeds this time interval",
                   UintegerValue (PROBE_PATH_EXPIRED_TIME_IN_NANOSECOND),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_pathExpiredTimeThld),
                   MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("pathSelStrategy", " how to select the path based on the latency",
                   UintegerValue (PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_pathSelStrategy),
                   MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("reorderFlag", " reorderFlag or not",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_reorderFlag),
                   MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("piggybackStrategy", " how to take away the piggyback data",
                   UintegerValue (PIGGY_BACK_SMALL_SENT_TIME_FIRST_STRATEGY),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_piggybackStrategy),
                   MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("probePathStrategy", " how to select the path to probe",
                   UintegerValue (PROBE_SMALL_LATENCY_FIRST_STRATEGY),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_probeStrategy),
                   MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("flowletTimoutInNs", " flowlet Timout In Ns",
                   UintegerValue (FLOWLET_DEFAULT_TIMEOUT_IN_NANOSECOND),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_flowletTimoutInNs),
                   MakeUintegerChecker<uint32_t> ())
      .AddAttribute ("pathSelNum", " path selection number",
                   UintegerValue (DEFAULT_PATH_SELECTION_NUM),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_pathSelNum),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("EnableReorderTracing",
                   "Enable the tracing the reorder",
                   BooleanValue (true),
                   MakeBooleanAccessor (&Ipv4SmartFlowRouting::m_reorderTraceEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableProbeTracing",
                   "Enable the tracing the probe packets",
                   BooleanValue (true),
                   MakeBooleanAccessor (&Ipv4SmartFlowRouting::m_probeTraceEnbale),
                   MakeBooleanChecker ())
      .AddAttribute ("piggyLatencyCnt", " piggy back Latency count",
                   UintegerValue (PIGGY_BACK_DEFAULT_PATH_NUMBER),
                   MakeUintegerAccessor (&Ipv4SmartFlowRouting::m_piggyLatencyCnt),
                   MakeUintegerChecker<uint32_t> ());    
    return tid;
}   

Ptr<Ipv4Route> Ipv4SmartFlowRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) {
    NS_LOG_ERROR (this << " SmartFlow routing is not support for local routing output");
    return 0;
}

vmt_entry_t * Ipv4SmartFlowRouting::lookup_VMT (const Ipv4Address &serverAddr) {
    // NS_LOG_INFO ("############ Fucntion: lookup_VMT() ############");
    // NS_LOG_INFO("VMT_KEY: svAddr=" << serverAddr);
    vmt_entry_t vmtEntry;
    std::map<Ipv4Address, vmt_entry_t>::iterator it;
    it = m_vmVtepMapTbl.find(serverAddr);
    if (it == m_vmVtepMapTbl.end()) {
        std::cout << "Error in lookup_VMT() since Cannot match any entry in VMT for the Key: (";
        std::cout << ipv4Address_to_string(serverAddr);
        std::cout << std::endl;
        return 0;
    }else{
        return &(it->second);
    }
}

uint32_t Ipv4SmartFlowRouting::print_PIT(){
    uint32_t pitSize = m_nexthopSelTbl.size();
    uint32_t nodeID = get_node_id();
    std::cout << "Node " << nodeID << " has a PIT of " << pitSize << " entries" << std::endl;
    std::cout << "Index" << construct_target_string(2, " ");
    std::cout << "Pid" << construct_target_string(3, " ");
    std::cout << "Priority" << construct_target_string(2, " ");
    std::cout << "Latency" << construct_target_string(3, " ");
    std::cout << "GenTime" << construct_target_string(3, " ");
    std::cout << "PbnTime" << construct_target_string(3, " ");
    std::cout << "SntTime" << construct_target_string(3, " ");
    std::cout << "NodeID" << construct_target_string(20, " ");
    std::cout << "PortID" << construct_target_string(1, " ");
    std::cout << std::endl;
    uint32_t pathIdx = 0;
    std::map<uint32_t, PathData>::iterator it;
    for (it = m_nexthopSelTbl.begin(); it != m_nexthopSelTbl.end(); it++){
        std::cout << pathIdx << construct_target_string(5+2-to_string(pathIdx).size(), " ");
        PathData * pstEntry = &(it->second);
        uint32_t pid = pstEntry->pid;
        std::cout << pid << construct_target_string(3+3-to_string(pid).size(), " ");
        uint32_t priority = pstEntry->priority;
        std::cout << priority << construct_target_string(8+2-to_string(priority).size(), " ");
        uint32_t latency = pstEntry->latency;
        std::cout << latency << construct_target_string(7+3-to_string(latency).size(), " ");
        uint32_t GenTime = pstEntry->tsGeneration.GetNanoSeconds();
        std::cout << GenTime << construct_target_string(7+3-to_string(GenTime).size(), " ");
        uint32_t PbnTime = pstEntry->tsProbeLastSend.GetNanoSeconds();
        std::cout << PbnTime << construct_target_string(7+3-to_string(PbnTime).size(), " ");
        uint32_t SntTime = pstEntry->tsLatencyLastSend.GetNanoSeconds();
        std::cout << SntTime << construct_target_string(7+3-to_string(SntTime).size(), " ");
        std::string nodeIdsStr = vector_to_string<uint32_t>(pstEntry->nodeIdSequence);
        std::cout << nodeIdsStr << construct_target_string(20+6-to_string(nodeIdsStr).size(), " ");
        std::string portsStr = vector_to_string<uint32_t>(pstEntry->portSequence);
        std::cout << portsStr << construct_target_string(1, " ");
        std::cout<<std::endl;
        pathIdx = pathIdx + 1;

    }
    return pathIdx;
}

uint32_t Ipv4SmartFlowRouting::print_PST() {
    uint32_t nodeID = get_node_id();
    uint32_t pstSize = m_pathSelTbl.size();
    std::cout << "Node: **" << nodeID << "** has PST in smartFlow with **"<< pstSize << "** entries" << std::endl;
    std::cout << "Index" << construct_target_string(5, " ");
    std::cout << "(SrcTorAddr, DstToRAddr)" << construct_target_string(5, " ");
    std::cout << "PathNum" << construct_target_string(5, " ");
    std::cout << "LastIdx" << construct_target_string(2, " ");
    std::cout << "highestIdx" << construct_target_string(2, " ");;
    std::cout << "PathIDs" << construct_target_string(5, " ");;
    std::cout << std::endl;
    uint32_t entryCnt = 0;
    std::map<PathSelTblKey, pstEntryData>::const_iterator it;
    for ( it= m_pathSelTbl.begin(); it != m_pathSelTbl.end(); it++) {
        std::cout << entryCnt << construct_target_string(5+5-to_string(entryCnt).size(), " ");
        std::string srcTorAddr = ipv4Address_to_string(it->first.selfToRIp);
        std::string dstTorAddr = ipv4Address_to_string(it->first.dstToRIp);
        std::string pathKeyStr = "(" + srcTorAddr + ", " + dstTorAddr + ")";
        std::cout << pathKeyStr << construct_target_string(5+24-to_string(pathKeyStr).size(), " ");
        uint32_t curPathNum = it->second.pathNum;
        std::cout << curPathNum << construct_target_string(5+7-to_string(curPathNum).size(), " ");
        uint32_t lastSelectedPathIdx = it->second.lastSelectedPathIdx;
        std::cout << lastSelectedPathIdx << construct_target_string(7+2-to_string(lastSelectedPathIdx).size(), " ");
        uint32_t highestPriorityPathIdx = it->second.highestPriorityPathIdx;
        std::cout << highestPriorityPathIdx << construct_target_string(10+2-to_string(highestPriorityPathIdx).size(), " ");
        std::string pathIdsStr = vector_to_string<uint32_t>(it->second.paths);
        std::cout << pathIdsStr << construct_target_string(1, " ");
        std::cout << std::endl;
        entryCnt = entryCnt + 1;
    }
    return entryCnt;
}

  uint32_t Ipv4SmartFlowRouting::print_VMT(){
    uint32_t nodeID = get_node_id();
    uint32_t vmtSize = m_vmVtepMapTbl.size();
    std::cout << "Node: **" << nodeID << "** has VMT in smartFlow with **"<< vmtSize << "** entries" << std::endl;
    std::cout << "Index" << construct_target_string(5, " ");
    std::cout << "Server Address" << construct_target_string(6, " ");
    std::cout << "ToR Address" << construct_target_string(1, " ");
    std::cout << std::endl;
    uint32_t entryCnt = 0;
    std::map<Ipv4Address, vmt_entry_t>::const_iterator it;
    for ( it= m_vmVtepMapTbl.begin(); it != m_vmVtepMapTbl.end(); it++) {
        std::cout << entryCnt << construct_target_string(5+5-to_string(entryCnt).size(), " ");
        std::cout << ipv4Address_to_string(it->first) << construct_target_string(6+14-ipv4Address_to_string(it->first).size(), " ");
        std::cout << ipv4Address_to_string(it->second.torAddr) << construct_target_string(1, " ");
        std::cout << std::endl;
        entryCnt = entryCnt + 1;

    }
    return entryCnt;
    
  }

uint32_t Ipv4SmartFlowRouting::CalculateQueueLength (uint32_t interface) {
  Ptr<Ipv4L3Protocol> ipv4L3Protocol = DynamicCast<Ipv4L3Protocol> (m_ipv4);
  if (!ipv4L3Protocol)  {
    std::cout<<" dragflow routing cannot work other than Ipv4L3Protocol" << std::endl;
    return 0;
  }

  uint32_t totalLength = 0;
  const Ptr<NetDevice> netDevice = this->m_ipv4->GetNetDevice (interface);
  if (netDevice->IsPointToPoint ()) {
    Ptr<PointToPointNetDevice> p2pNetDevice = DynamicCast<PointToPointNetDevice> (netDevice);
    if (p2pNetDevice) {
          totalLength += p2pNetDevice->GetQueue()->GetNBytes(); // GetNPackets
          // std::cout<<"'s QueueLen is "<<totalLength<<std::endl;

    }
  }

//   Ptr<TrafficControlLayer> tc = ipv4L3Protocol->GetNode ()->GetObject<TrafficControlLayer> ();

//   if (!tc)
//   {
//     NS_LOG_INFO (this << " no such process! ");
//     return totalLength;
//   }

//   Ptr<QueueDisc> queueDisc = tc->GetRootQueueDiscOnDevice (netDevice);
//   if (queueDisc)
//   {
//     totalLength += queueDisc->GetNBytes ();
//     // std::cout<<"################ The Number of Pkts in Queue and Disc is "<<totalLength<<std::endl;

//   }

  return totalLength;
}



Ptr<Ipv4Route> Ipv4SmartFlowRouting::ConstructIpv4Route (uint32_t port, Ipv4Address &destAddress) {
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (port); // get the netdevice linked to the port-th port/netdevice获取连接到端口的网络设备-端口/网络设备
  Ptr<Channel> channel = dev->GetChannel (); // get the channel linked to the port-th port/获取与 port-th 端口相连的通道
  uint32_t otherEnd = (channel->GetDevice (0) == dev) ? 1 : 0; // 查看对端是这个channel的0或1
  Ptr<Node> nextHop = channel->GetDevice (otherEnd)->GetNode (); // 找到下一跳节点node
  uint32_t nextIf = channel->GetDevice (otherEnd)->GetIfIndex (); // 查看是对端node的第几个netdevice
  Ipv4Address nextHopAddr = nextHop->GetObject<Ipv4>()->GetAddress(nextIf,0).GetLocal();
  Ptr<Ipv4Route> route = Create<Ipv4Route> ();
  route->SetOutputDevice (m_ipv4->GetNetDevice (port)); // 这条规则的报文从哪个网卡出去
  route->SetGateway (nextHopAddr); // 默认网关是下一跳
  route->SetSource (m_ipv4->GetAddress (port, 0).GetLocal ());
  route->SetDestination (destAddress);
  return route;
}

bool Ipv4SmartFlowRouting::output_packet_by_port_id(Ptr<const Packet> p, const Ipv4Header &header, UnicastForwardCallback ucb, uint32_t selectedPort, Ipv4Address &dstServerAddr) {
    // NS_LOG_INFO ("############ Fucntion: output_packet_by_port_id() ############");
    // NS_LOG_INFO ("The packet size is " << p->GetSize());
    Ptr<Ipv4Route> route = ConstructIpv4Route(selectedPort, dstServerAddr);
    ucb(route, p, header);
    // NS_LOG_INFO ("The packet size is " << p->GetSize());
    return true;
}

bool Ipv4SmartFlowRouting::output_packet_by_path_tag(Ptr<Packet> packet, const Ipv4Header &header, UnicastForwardCallback ucb, Ipv4Address &dstServerAddr) {

    // NS_LOG_INFO ("############ Fucntion: output_packet_by_path_tag() ############");
    // std::cout << "Enter output_packet_by_path_tag()"<<std::endl;

    Ipv4SmartFlowPathTag pathTag;
    packet->PeekPacketTag(pathTag);
    // uint32_t maxQLen = pathTag.GetDre();
    uint32_t selectedPortId = get_egress_port_id_by_path_tag(pathTag);
    // if (selectedPortId >= m_pathhitTable.size()) {
    //     m_pathhitTable.resize(selectedPortId+1, 0);
    //     m_pathhitTable[selectedPortId] = 1;
    // }else{
    //     m_pathhitTable[selectedPortId] += 1;
    // }
    

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
    if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
        UpdateLocalDre (header, packet, selectedPortId);
    }
    
    output_packet_by_port_id(packet, header, ucb, selectedPortId, dstServerAddr);
        // std::cout << "Leave output_packet_by_path_tag()"<<std::endl;

    return true;
}

void Ipv4SmartFlowRouting::add_conga_tag_by_pit_entry (Ptr<Packet> packet, PathData * pitEntry) {
    if (pitEntry == 0)  {
        return ;
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


}


void Ipv4SmartFlowRouting::add_latency_tag_by_pit_entries (Ptr<Packet> packet, std::vector<PathData *> &pitEntries) {
    // NS_LOG_INFO ("############ Fucntion: add_latency_tag_by_pit_entries() ############");
    uint32_t pathNum = pitEntries.size();
    if (pathNum == 0 || m_piggyLatencyCnt == 0)  {
        NS_LOG_INFO ("No need to add latency tag since pathNum=" << pathNum << ", m_piggyCnt=" << m_piggyLatencyCnt);
        return;
    }else {
        for (uint32_t i = 0; i < pathNum; i++) {
            if (i+1 > m_piggyLatencyCnt) {
                break;
            }
            if(i == 0){
                Ipv4SmartFlowLatencyTag<0> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 1){
                Ipv4SmartFlowLatencyTag<1> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 2){
                Ipv4SmartFlowLatencyTag<2> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 3){
                Ipv4SmartFlowLatencyTag<3> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 4){
                Ipv4SmartFlowLatencyTag<4> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 5){
                Ipv4SmartFlowLatencyTag<5> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 6){
                Ipv4SmartFlowLatencyTag<6> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 7){
                Ipv4SmartFlowLatencyTag<7> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 8){
                Ipv4SmartFlowLatencyTag<8> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }else if(i == 9){
                Ipv4SmartFlowLatencyTag<9> latencyTag;
                latencyTag.set_data_by_pit_entry(pitEntries[i]);
                packet->AddPacketTag(latencyTag);
            }
        }
    }
    return;
}

void Ipv4SmartFlowRouting::update_PIT_after_adding_path_tag(PathData * piggyBackPitEntry){
    // NS_LOG_INFO ("############ Fucntion: update_PIT_after_adding_path_tag() ############");
    if (m_pathSelStrategy == PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY) {
        piggyBackPitEntry->tsProbeLastSend = Simulator::Now();
    }    
}

void Ipv4SmartFlowRouting::update_PST_after_adding_path_tag(pstEntryData* pstEntry, PathData * forwardPitEntry){
    // NS_LOG_INFO ("############ Fucntion: update_PST_after_adding_path_tag() ############");
    if (m_pathSelStrategy == PATH_SELECTION_FLOWLET_STATEGY) {
        uint32_t pathCnt = pstEntry->paths.size();
        for (uint32_t i = 0; i < pathCnt; i++) {
            if (pstEntry->paths[i] == forwardPitEntry->pid) {
                pstEntry->lastSelectedPathIdx = i;
                pstEntry->lastSelectedTimeInNs = Simulator::Now().GetNanoSeconds();
                return ;            
            }
        }
        NS_LOG_INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Error in update_PST_after_adding_path_tag due to flowlet strategy");
    }else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_PACKET_STRATEGY) {
        if (pstEntry->lastSelectedPathIdx == DEFAULT_PATH_INDEX) {
            pstEntry->lastSelectedPathIdx =0;
        }else{
            pstEntry->lastSelectedPathIdx = (pstEntry->lastSelectedPathIdx+1) % pstEntry->pathNum;
        }
    }
}

uint32_t Ipv4SmartFlowRouting::update_PIT_after_piggybacking(std::vector<PathData *> &piggyBackPitEntries){
    // NS_LOG_INFO ("############ Fucntion: update_PIT_after_piggybacking() ############");
    uint32_t pathNum = piggyBackPitEntries.size();
    for (uint32_t i = 0; i < pathNum; i++) {
        piggyBackPitEntries[i]->tsLatencyLastSend = Simulator::Now();
    }
    return pathNum;
}




Ipv4SmartFlowPathTag Ipv4SmartFlowRouting::construct_path_tag (uint32_t selectedPathId) {
    // NS_LOG_INFO ("############ Fucntion: construct_path_tag() ############");
    Ipv4SmartFlowPathTag pathTag;
    pathTag.SetPathId(selectedPathId);
    pathTag.set_hop_idx(0);
    Time now = Simulator::Now();
    pathTag.SetTimeStamp(now);
    pathTag.SetDre(0);
    return pathTag;
}

PathData * Ipv4SmartFlowRouting::get_the_smallest_latency_path (std::vector<PathData *> &pitEntries) {
    // NS_LOG_INFO ("############ Fucntion: get_the_smallest_latency_path() ############");
    if (pitEntries.size() == 0) {
        return 0;
    }
    std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_latency);
    return pitEntries[0];
}



PathData * Ipv4SmartFlowRouting::get_the_oldest_measured_path (std::vector<PathData *> &pitEntries) {
    // NS_LOG_INFO ("############ Fucntion: get_the_oldest_measured_path() ############");
    if (pitEntries.size() == 0) {
        return 0;
    }
    std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_Generation_time);
    return pitEntries[0];
}

PathData * Ipv4SmartFlowRouting::get_the_random_path(std::vector<PathData *> &pitEntries) {
    // NS_LOG_INFO ("############ Fucntion: get_the_random_path() ############");
    if (pitEntries.size() == 0) {
        return 0;
    }
    PathData *  bestPitEntry = pitEntries[std::rand() % pitEntries.size()];
    return bestPitEntry;
}



uint32_t Ipv4SmartFlowRouting::get_the_routing_index(){
    Ptr<Node> node = m_ipv4->GetObject<Node> ();
    Ptr<Ipv4RoutingProtocol> ptr_rt = 0;
    Ptr<Ipv4ListRouting> ptr_lrt = 0;
    ptr_rt = node->GetObject<Ipv4>()->GetRoutingProtocol();
    ptr_lrt = DynamicCast<Ipv4ListRouting>(ptr_rt);
    if (ptr_lrt == 0) {
        std::cout << "Error in get_the_routing_index()-->do not install ListRoutingProtocol" << std::endl;
        return 0;
    }
    int16_t priority = 0;
    for (uint32_t i = 0; i < ptr_lrt->GetNRoutingProtocols(); i++) {
        Ptr<Ipv4RoutingProtocol> listProto = ptr_lrt->GetRoutingProtocol(i, priority);
        Ptr<Ipv4SmartFlowRouting> flag = DynamicCast<Ipv4SmartFlowRouting>(listProto);
        if (flag != 0) {
            return i;
        }
    }
    return 0;
}

Ptr<Ipv4SmartFlowRouting> Ipv4SmartFlowRouting::get_the_routing_protocol(){
    Ptr<Node> node = m_ipv4->GetObject<Node> ();
    Ptr<Ipv4RoutingProtocol> ipv4Prot = node->GetObject<Ipv4>()->GetRoutingProtocol();
    Ptr<Ipv4ListRouting> listProt = DynamicCast<Ipv4ListRouting>(ipv4Prot);//将类型Ipv4RoutingProtocol转换为Ipv4ListRouting
    if (listProt == 0) {
        std::cout << "ERROR in get_the_routing_protocol(), MUST install ipv4ListRouting first" << std::endl;
        return 0;
    }
    uint32_t index =  get_the_routing_index();
    int16_t priority = 0;
    ipv4Prot = listProt->GetRoutingProtocol(index, priority);
    Ptr<Ipv4SmartFlowRouting> smartflowProt = DynamicCast<Ipv4SmartFlowRouting>(ipv4Prot);
    return smartflowProt;
}


PathData * Ipv4SmartFlowRouting::get_the_hashing_path(std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header) {
    NS_LOG_INFO("get_the_hashing_path()");
    uint32_t flowId = hashing_flow_with_5_tuple(packet, header);
    uint32_t pathIdx = flowId % pitEntries.size();
    PathData *  bestPitEntry = pitEntries[pathIdx];
    if (bestPitEntry == 0) {
        std::cout << "Error in get_the_hashing_path() about the zere ruturn value" << std::endl;
        return 0;
    }
    return bestPitEntry;
}

PathData * Ipv4SmartFlowRouting::get_the_round_robin_path_for_hybird(pstEntryData* pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header) {
    NS_LOG_INFO("get_the_round_robin_path_for_hybird()");
    std::string flowStr = hashing_flow_with_5_tuple_to_string(packet, header);
    // NS_LOG_INFO("Flow INFO : "<< flowStr);
    std::map<std::string, rbn_entry_t >::iterator it;
    it = m_rbnTbl.find(flowStr);
    PathData *  bestPitEntry = 0;
    if (it == m_rbnTbl.end()) {
        // std::cout << "New Flow" << std::endl;
        if (pstEntry->lastSelectedPathIdx == DEFAULT_PATH_INDEX) {
            pstEntry->lastSelectedPathIdx = 0;
        }else{
            pstEntry->lastSelectedPathIdx = (pstEntry->lastSelectedPathIdx+1) % pstEntry->pathNum;
        }
        bestPitEntry = pitEntries[pstEntry->lastSelectedPathIdx];
        rbn_entry_t rbnEnty = rbn_entry_t();
        rbnEnty.lastSelectedPathIdx = 0;
        m_rbnTbl[flowStr] = rbnEnty;
    }else{
        NS_LOG_INFO("Exist Flow");

        uint32_t pathNum = pstEntry->pathNum;
        uint32_t curSelectedPathIdx = (it->second.lastSelectedPathIdx + 1) % pathNum;
        bestPitEntry = pitEntries[curSelectedPathIdx];
        it->second.lastSelectedPathIdx = curSelectedPathIdx;
    }
    return bestPitEntry;
}

PathData * Ipv4SmartFlowRouting::get_the_round_robin_path_for_flow(pstEntryData* pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header) {
    NS_LOG_INFO("get_the_round_robin_path_for_flow()");  
    std::string flowStr = hashing_flow_with_5_tuple_to_string(packet, header);
    NS_LOG_INFO("Flow INFO : "<< flowStr);
    std::map<std::string, rbn_entry_t >::iterator it;
    it = m_rbnTbl.find(flowStr);
    PathData *  bestPitEntry = 0;
    if (it == m_rbnTbl.end()) {
        NS_LOG_INFO("New Flow");
        if (pstEntry->lastSelectedPathIdx == DEFAULT_PATH_INDEX) {
            pstEntry->lastSelectedPathIdx = 0;
        }else{
            pstEntry->lastSelectedPathIdx = (pstEntry->lastSelectedPathIdx+1) % pstEntry->pathNum;
        }
        bestPitEntry = pitEntries[pstEntry->lastSelectedPathIdx];
        rbn_entry_t rbnEnty = rbn_entry_t();
        rbnEnty.selectedPitEntry = bestPitEntry;
        m_rbnTbl[flowStr] = rbnEnty;
    }else{
        NS_LOG_INFO("Exist Flow");
        bestPitEntry = it->second.selectedPitEntry;
    }
    return bestPitEntry;
}


PathData * Ipv4SmartFlowRouting::get_the_round_robin_path_for_packet(pstEntryData* pstEntry, std::vector<PathData *> &pitEntries) {
    NS_LOG_INFO("get_the_round_robin_path_for_packet()");
    if (pstEntry->lastSelectedPathIdx == DEFAULT_PATH_INDEX) {
        pstEntry->lastSelectedPathIdx =0;
    }else{
        pstEntry->lastSelectedPathIdx = (pstEntry->lastSelectedPathIdx+1) % pstEntry->pathNum;
    }
        return pitEntries[pstEntry->lastSelectedPathIdx];
}

PathData * Ipv4SmartFlowRouting::get_the_flowlet_path(pstEntryData* pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header) {
    NS_LOG_INFO("get_the_flowlet_path()");
    std::string flowStr = hashing_flow_with_5_tuple_to_string(packet, header);
    NS_LOG_INFO("Flow INFO : "<< flowStr);
    std::map<std::string, flet_entry_t >::iterator it;
    it = m_fletTbl.find(flowStr);
    PathData *  bestPitEntry = 0;
    Time curTime = Simulator::Now();
    uint32_t curTimeInNs = curTime.GetNanoSeconds();
    uint32_t curSelectedPathIdx = std::rand() % pitEntries.size();
    if (it == m_fletTbl.end()) {
        NS_LOG_INFO("New Flow");
        bestPitEntry = pitEntries[curSelectedPathIdx];
        flet_entry_t fletEnty = flet_entry_t();
        fletEnty.lastSelectedPitEntry = bestPitEntry;
        fletEnty.lastSelectedTimeInNs =  curTimeInNs;
        m_fletTbl[flowStr] = fletEnty;
    }else{
        NS_LOG_INFO("Exist Flow");
        uint32_t gap = curTimeInNs - it->second.lastSelectedTimeInNs;
        NS_LOG_INFO("Flowlet Timout In Nano Second : " << m_flowletTimoutInNs << ", curGapInNs: " << gap);
        if (gap > m_flowletTimoutInNs) {
            NS_LOG_INFO("New Flowlet, changes the path");
            bestPitEntry = pitEntries[curSelectedPathIdx];
            it->second.lastSelectedPitEntry = bestPitEntry;
            it->second.lastSelectedTimeInNs = curTimeInNs;
        }else{
            NS_LOG_INFO("Same Flowlet, maintain the previous path");
            bestPitEntry = it->second.lastSelectedPitEntry;
            it->second.lastSelectedTimeInNs = curTimeInNs;
        }
    }
    return bestPitEntry;
}

PathData * Ipv4SmartFlowRouting::get_the_least_congested_path(pstEntryData* pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header){
    // std::cout << "Enter get_the_least_congested_path_for_conga()"<<std::endl;
    std::string flowStr = hashing_flow_with_5_tuple_to_string(packet, header);
    NS_LOG_INFO("Flow INFO : "<< flowStr);

    PathData *  bestPitEntry = 0;
    std::vector<PathData *> candidatePitEntries;
    uint32_t n = pitEntries.size(), minCongestionDegree=999999;
    std::vector<uint32_t> ports = get_egress_ports_from_pit_entries(pitEntries);
    std::vector<uint32_t> portDres = get_dre_of_egress_ports(ports);
    for (size_t i = 0; i < n; i++) {
        // pitEntries[i]->print();
        uint32_t tmpCongestionDegree = std::max (portDres[i], pitEntries[i]->pathDre);
        if (tmpCongestionDegree <= minCongestionDegree) {
            minCongestionDegree = tmpCongestionDegree;
        }
        // std::cout <<"All PathId: "<< pitEntries[i]->pid << ", remote Dre: " << pitEntries[i]->pathDre << ", local Dre: "<< portDres[i] << std::endl;
        // std::cout << tmpCongestionDegree << ", " << minCongestionDegree << std::endl;
    }
    for (size_t i = 0; i < n; i++) {
        uint32_t tmpCongestionDegree = std::max (portDres[i], pitEntries[i]->pathDre);
        if (tmpCongestionDegree == minCongestionDegree) {
            candidatePitEntries.push_back(pitEntries[i]);
            // std::cout <<"Candidate PathId: "<< pitEntries[i]->pid << ", remote Dre: " << pitEntries[i]->pathDre << ", local Dre: "<< portDres[i] << std::endl;

        }
    }
    if (candidatePitEntries.size() == 0) {
        std::cout << "Node " << m_nodeId << " Error in get_the_least_congested_path() without any valid path" << std::endl;
    }
    bestPitEntry = candidatePitEntries[std::rand() % candidatePitEntries.size ()];
            // std::cout <<"Best PathId: "<< bestPitEntry->pid << std::endl;

    // NS_LOG_INFO("candidate Paths:");
    // for (auto & it : candidatePitEntries) {
    //     it->print();
    // }
    // NS_LOG_INFO("Cur Best Paths:");
    // bestPitEntry->print();

    Time curTime = Simulator::Now();
    uint32_t curTimeInNs = curTime.GetNanoSeconds();
    std::map<std::string, flet_entry_t >::iterator it;
    it = m_fletTbl.find(flowStr);
    if (it == m_fletTbl.end()) {
        NS_LOG_INFO("New Flow");
        flet_entry_t fletEnty = flet_entry_t();
        fletEnty.lastSelectedPitEntry = bestPitEntry;
        fletEnty.lastSelectedTimeInNs =  curTimeInNs;
        m_fletTbl[flowStr] = fletEnty;
    }else{
        NS_LOG_INFO("Exist Flow");
        uint32_t gap = curTimeInNs - it->second.lastSelectedTimeInNs;
        NS_LOG_INFO("Flowlet Timout In Nano Second : " << m_flowletTimoutInNs << ", curGapInNs: " << gap);
        if (gap > m_flowletTimoutInNs) { // expired
            NS_LOG_INFO("New Flowlet, changes the path");
            it->second.lastSelectedPitEntry = bestPitEntry;
            it->second.lastSelectedTimeInNs = curTimeInNs;
        }else{ // fresh
            NS_LOG_INFO("Same Flowlet, maintain the previous path");
            bestPitEntry = it->second.lastSelectedPitEntry;
            it->second.lastSelectedTimeInNs = curTimeInNs;
        }
    }
    // std::cout << "Leave get_the_least_congested_path_for_conga()"<<std::endl;
    // NS_LOG_INFO("Final Best Paths:");
    // bestPitEntry->print();
    return bestPitEntry;

}

PathData * Ipv4SmartFlowRouting::get_the_highest_priority_path (std::vector<PathData *> &pitEntries) {
    PathData *  bestPitEntry = 0;
    std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_decrease_order_of_priority);
    bestPitEntry = pitEntries[0];
    return bestPitEntry;
}

PathData * Ipv4SmartFlowRouting::get_the_best_forwarding_path (std::vector<PathData *> & pitEntries, pstEntryData* pstEntry, Ptr<const Packet> packet, const Ipv4Header &header) {
    // NS_LOG_INFO ("############ Fucntion: get_the_best_forwarding_path() ############");
    PathData *  bestPitEntry = 0;
    if(m_pathSelStrategy == PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY) {
        bestPitEntry = get_the_smallest_latency_path(pitEntries); // smartflow
        // bestPitEntry->tsProbeLastSend = Simulator::Now();
    }else if (m_pathSelStrategy == PATH_SELECTION_RANDOM_STRATEGY) { // hashflow
        bestPitEntry = get_the_random_path(pitEntries);
    }else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_PACKET_STRATEGY) { // rbnflow
        bestPitEntry = get_the_round_robin_path_for_packet(pstEntry, pitEntries);
    }else if (m_pathSelStrategy == PATH_SELECTION_FLOW_HASH_STRATEGY) {  //ecmp
        bestPitEntry = get_the_hashing_path(pitEntries, packet, header);    
    }else if (m_pathSelStrategy == PATH_SELECTION_PRIORITY_FIRST_STRATEGY) { // minflow
        bestPitEntry = get_the_highest_priority_path(pitEntries);    
    }else if (m_pathSelStrategy == PATH_SELECTION_FLOWLET_STATEGY) {
         bestPitEntry = get_the_flowlet_path(pstEntry, pitEntries, packet, header);
    }else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_FLOW_STRATEGY) {
         bestPitEntry = get_the_round_robin_path_for_flow(pstEntry, pitEntries, packet, header);
    }else if (m_pathSelStrategy == PATH_SELECTION_ROUND_ROBIN_FOR_HYBRID_STRATEGY) {
         bestPitEntry = get_the_round_robin_path_for_hybird(pstEntry, pitEntries, packet, header);
    }else if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {

         bestPitEntry = get_the_least_congested_path(pstEntry, pitEntries, packet, header); // conga
    }
    return bestPitEntry;
}

std::vector<PathData *> Ipv4SmartFlowRouting::get_the_best_piggyback_paths (std::vector<PathData *> & pitEntries) {
    // NS_LOG_INFO ("############ Fucntion: get_the_best_reverse_paths() ############");
    std::vector<PathData *> bestPitEntries;
    bestPitEntries.clear();
    // NS_LOG_INFO("The results after apply piggyback strategy=" << m_piggybackStrategy << " are:{0:latencyFirst},  {1:gentFirst}, {2:sentFirst}");
    if (m_piggybackStrategy == PIGGY_BACK_SMALL_LATENCY_FIRST_STRATEGY) {
        std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_latency);
    }else if (m_piggybackStrategy == PIGGY_BACK_SMALL_GENT_TIME_FIRST_STRATEGY) {
        std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_Generation_time);
    }else if (m_piggybackStrategy == PIGGY_BACK_SMALL_SENT_TIME_FIRST_STRATEGY) {
        std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_increase_order_of_Sent_time);
    }else{
        std::cout << "Error in get_the_best_piggyback_paths() since m_probeStrategy=" << m_piggybackStrategy << std::endl;
        return bestPitEntries;
    }
    uint32_t allCnt = pitEntries.size();
    if (allCnt <= m_piggyLatencyCnt) {
        bestPitEntries.assign(pitEntries.begin(), pitEntries.end());
    }else{
        bestPitEntries.assign(pitEntries.begin(), pitEntries.begin() + m_piggyLatencyCnt);
    }
    return bestPitEntries;
}

uint32_t Ipv4SmartFlowRouting::get_the_expired_paths (std::vector<PathData *> & allPitEntries, std::vector<PathData *> & expiredPitEntries) {
    // NS_LOG_INFO ("############ Fucntion: get_the_expired_paths() ############");
    expiredPitEntries.clear();
    int64_t curTime = Simulator::Now().GetNanoSeconds();
    for(auto e : allPitEntries){
        if((curTime - e->tsGeneration.GetNanoSeconds()) > (e->theoreticalSmallestLatencyInNs*2+LINK_LATENCY_IN_NANOSECOND)){
            expiredPitEntries.push_back(e);
        }
    }
    return expiredPitEntries.size();
}

uint32_t Ipv4SmartFlowRouting::get_the_potential_paths (PathData * bestPitEntry, std::vector<PathData *> & allPitEntries, std::vector<PathData *> & potentialPitEntries) {
    // NS_LOG_INFO ("############ Fucntion: get_the_potential_paths() ############");
    potentialPitEntries.clear();
    for(auto e : allPitEntries){
        if(bestPitEntry->latency > e->theoreticalSmallestLatencyInNs){
            potentialPitEntries.push_back(e);
        }
    }
    return potentialPitEntries.size();
}

uint32_t Ipv4SmartFlowRouting::get_the_probe_paths (std::vector<PathData *> & expiredPitEntries, std::vector<PathData *> & probePitEntries) {
    // NS_LOG_INFO ("############ Fucntion: get_the_probe_paths() ############");
    probePitEntries.clear();
    int64_t curTime = Simulator::Now().GetNanoSeconds();
    for(auto e : expiredPitEntries){
        if((curTime - e->tsProbeLastSend.GetNanoSeconds()) > (e->theoreticalSmallestLatencyInNs*2+LINK_LATENCY_IN_NANOSECOND)){
            probePitEntries.push_back(e);
        }
    }
    return probePitEntries.size();
}

PathData * Ipv4SmartFlowRouting::get_the_best_probing_path (std::vector<PathData *> & pitEntries) {
    // NS_LOG_INFO("The results after apply probing strategy=" << m_probeStrategy << " are:{0:latencyFirst}, {1: gentFirst}, {2:random}");

    // NS_LOG_INFO ("############ Fucntion: get_the_best_probing_path() ############");
    if (pitEntries.size() == 0) {
        return 0;
    }                
    if (m_probeStrategy == PROBE_SMALL_LATENCY_FIRST_STRATEGY) {
        return get_the_smallest_latency_path(pitEntries);
    }else if (m_probeStrategy == PROBE_RANDOM_STRATEGY) { // hashflow
        return get_the_random_path(pitEntries);
    }else if (m_probeStrategy == PROBE_SMALL_GENERATION_TIME_FIRST_STRATEGY) {
        return get_the_oldest_measured_path(pitEntries);
    }
    return 0;
}

void Ipv4SmartFlowRouting::initialize(){
    // highestPriorityPathIdx
    for (auto & pstEntry:  m_pathSelTbl) {
        std::vector<PathData *> pitEntries = batch_lookup_PIT(pstEntry.second.paths);
        std::sort(pitEntries.begin(), pitEntries.end(), cmp_pitEntry_in_decrease_order_of_priority);
        uint32_t highestPriorityPathIdx = 0;
        for (uint32_t i = 1; i < pstEntry.second.paths.size(); i++) {
            if (pstEntry.second.paths[i] == pitEntries[0]->pid) {
                highestPriorityPathIdx = i;
                break;
            }
        }
        pstEntry.second.highestPriorityPathIdx = highestPriorityPathIdx;
        pstEntry.second.lastSelectedPathIdx = DEFAULT_PATH_INDEX;
        pstEntry.second.lastPiggybackPathIdx = 0;     
    }
    for (auto & pitEntry:  m_nexthopSelTbl) {
        pitEntry.second.theoreticalSmallestLatencyInNs = pitEntry.second.portSequence.size()*LINK_LATENCY_IN_NANOSECOND;
        pitEntry.second.pathDre = 0;   // to be modified 
    }
    // m_nodeId
    m_nodeId = get_node_id();
    for (size_t i = 1; i < CONGA_SW_PORT_NUM; i++) {
        m_XMap[i] = 0;
    }
    // lastSelectedPathIdx
    
}


std::vector<PathData *> Ipv4SmartFlowRouting::get_the_newly_measured_paths (std::vector<PathData *> & pitEntries){      
    // NS_LOG_INFO ("############ Fucntion: get_the_newly_measured_paths() ############");                
    std::vector<PathData *> candidatePaths;
    candidatePaths.clear();
    uint32_t pathNum = pitEntries.size();
    for (uint32_t i = 0; i < pathNum; i++) {
        Time tsGeneration = pitEntries[i]->tsGeneration;
        Time tsLatencyLastSend = pitEntries[i]->tsLatencyLastSend;
        if (tsGeneration.GetNanoSeconds() > tsLatencyLastSend.GetNanoSeconds()) {
            candidatePaths.push_back(pitEntries[i]);
        }
    }
    return candidatePaths;
}

uint32_t Ipv4SmartFlowRouting::install_PIT(std::map<uint32_t, PathData> & pit){
    set_PIT(pit);
    return pit.size();
  }

uint32_t Ipv4SmartFlowRouting::install_PST(std::map<PathSelTblKey, pstEntryData> &pst){
    set_PST(pst);
    return pst.size();
  }

uint32_t Ipv4SmartFlowRouting::install_VMT(std::map<Ipv4Address, vmt_entry_t>  &vmt){
    set_VMT(vmt);
    return vmt.size();
  }



void Ipv4SmartFlowRouting::update_PIT_by_latency_data(LatencyData &latencyData) {
    // NS_LOG_INFO ("############ Fucntion: update_PIT_by_latency_data() ############");                
    uint32_t pid = latencyData.latencyInfo.first;
    uint32_t newLatency = latencyData.latencyInfo.second;
    Time newLatencyGeneration = latencyData.tsGeneration;
    PathData * pitEntry = lookup_PIT(pid);
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

    if (newLatencyGeneration > oldLatencyGeneration) {
        pitEntry->latency = newLatency;
        pitEntry->tsGeneration = newLatencyGeneration;
    }else{
        NS_LOG_INFO ("!!!!!!!!!!!!!!SPECIAL CASE: older LatencyINFO overrides the newly one!!!!!!!!!!!!!!!");
    }
}



void Ipv4SmartFlowRouting::update_PIT_by_latency_tag(Ptr<Packet> &packet) {
    // NS_LOG_INFO ("############ Fucntion: update_PIT_by_latency_tag() ############");                
    for (uint32_t i = 0; i < m_piggyLatencyCnt; i++) {
        if (i == 0) {
            Ipv4SmartFlowLatencyTag<0> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 1) {
            Ipv4SmartFlowLatencyTag<1> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 2) {
            Ipv4SmartFlowLatencyTag<2> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 3) {
            Ipv4SmartFlowLatencyTag<3> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 4) {
            Ipv4SmartFlowLatencyTag<4> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 5) {
            Ipv4SmartFlowLatencyTag<5> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 6) {
            Ipv4SmartFlowLatencyTag<6> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 7) {
            Ipv4SmartFlowLatencyTag<7> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 8) {
            Ipv4SmartFlowLatencyTag<8> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }else if (i == 9) {
            Ipv4SmartFlowLatencyTag<9> latencyTag;
            if (packet->PeekPacketTag(latencyTag)) {
                LatencyData latencyData = latencyTag.GetLatencyData();
                update_PIT_by_latency_data(latencyData);
            }else{
                break;
            }
        }

    }
    return;
}


void Ipv4SmartFlowRouting::update_PIT_by_path_tag(Ipv4SmartFlowPathTag & pathTag, PathData * & pitEntry) {
    // NS_LOG_INFO ("############ Fucntion: update_PIT_by_path_tag() ############");
    if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
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
    }
    Time curTime = Simulator::Now();
    Time oldTime = pathTag.GetTimeStamp();
    uint32_t newLatency = (uint32_t)(curTime.GetNanoSeconds() - oldTime.GetNanoSeconds());
    pitEntry->latency = newLatency;
    pitEntry->tsGeneration = curTime;
    return;
}

void Ipv4SmartFlowRouting::update_PIT_by_conga_tag(Ipv4SmartFlowCongaTag & congaTag) {
    // NS_LOG_INFO ("############ Fucntion: update_PIT_by_path_tag() ############");
    uint32_t pid = congaTag.get_path_id();
    uint32_t pathDre = congaTag.GetDre();
    PathData * pitEntry = lookup_PIT(pid);
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
}


void Ipv4SmartFlowRouting::update_PIT_by_probe_tag(Ipv4SmartFlowProbeTag &probeTag) {
    // NS_LOG_INFO ("############ Fucntion: update_PIT_by_probe_tag() ############");
    uint32_t pid, newLatency;
    probeTag.get_path_info(pid, newLatency);
    PathData * pitEntry = lookup_PIT(pid);
    pitEntry->latency = newLatency;
    pitEntry->tsGeneration = Simulator::Now();
    return;
}

void Ipv4SmartFlowRouting::receive_probe_packet(Ipv4SmartFlowProbeTag &probeTag){
    // NS_LOG_INFO ("############ Fucntion: receive_probe_packet() ############");
    if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
        std::cout<< "un expected probe packet in conga" << std::endl;
        return ;
    }
    
    update_PIT_by_probe_tag(probeTag);
    return ;

}  

void Ipv4SmartFlowRouting::update_path_tag(Ptr<Packet> &packet, Ipv4SmartFlowPathTag &pathTag) {
    // NS_LOG_INFO ("############ Fucntion: update_path_tag() ############");
    uint32_t curHopIdx = pathTag.get_the_current_hop_index();
    if (curHopIdx == 0) {
        uint32_t pathId = pathTag.get_path_id();
       if (pathId >= m_pathhitTable.size()) {
            m_pathhitTable.resize(pathId+1, 0);
            m_pathhitTable[pathId] = 1;
        }else{
            m_pathhitTable[pathId] += 1;
        }
    }
    
    pathTag.set_hop_idx(curHopIdx+1);
    packet->ReplacePacketTag (pathTag);
    return;
}

Ptr<Packet> Ipv4SmartFlowRouting::construct_probe_packet(Ptr<Packet> &pkt){
    Ptr<Packet> probePacket = Create<Packet> (PROBE_DEFAULT_PKT_SIZE_IN_BYTE);
    return probePacket;
  }

Ipv4SmartFlowProbeTag Ipv4SmartFlowRouting::construct_probe_tag_by_path_id(uint32_t expiredPathId){
    // NS_LOG_INFO ("############ Fucntion: construct_probe_tag_by_path_id() ############");
    Ipv4SmartFlowProbeTag probeTag;
    probeTag.Initialize(expiredPathId);
    return probeTag;
  }

pstEntryData * Ipv4SmartFlowRouting::lookup_PST(PathSelTblKey & pstKey){
    // NS_LOG_INFO ("############ Fucntion: lookup_PST() ############");
    std::map<PathSelTblKey, pstEntryData >::iterator it;
    it = m_pathSelTbl.find(pstKey);
    if (it == m_pathSelTbl.end()) {
        std::cout << "Error in lookup_PST() since Cannot match any entry in PST for the Key: (";
        std::cout << ipv4Address_to_string(pstKey.selfToRIp);
        std::cout << ", " << ipv4Address_to_string(pstKey.dstToRIp) << ")" << std::endl;
        return 0;
    }else{
        return &(it->second);
    }
}

pdt_entry_t * Ipv4SmartFlowRouting::lookup_PDT(PathSelTblKey & pstKey){
    NS_LOG_INFO ("############ Fucntion: lookup_PDT() ############");
    auto it = m_pathDecTbl.find(pstKey);
    if (it == m_pathDecTbl.end()) {
        NS_LOG_INFO("No matching entry in PDT for the following pstKey");
        // pstKey.print();
        return 0;
    }else{
        return &(it->second);
    }
}


PathData * Ipv4SmartFlowRouting::lookup_PIT(uint32_t  pieKey){
    // NS_LOG_INFO ("############ Fucntion: lookup_PIT() ############");
    std::map<uint32_t, PathData>::iterator it;
    it = m_nexthopSelTbl.find(pieKey);
    if (it == m_nexthopSelTbl.end()) {
        std::cout << "Error in lookup_PIT() since Cannot match any entry in PST for the Key: ";
        std::cout << pieKey;
        std::cout << std::endl;
        return 0;
    }else{
        return &(it->second);
    }
}
std::vector<PathData *> Ipv4SmartFlowRouting::batch_lookup_PIT(std::vector<uint32_t>& pids) {
    // NS_LOG_INFO ("############ Fucntion: batch_lookup_PIT() ############");
    std::vector<PathData *> pitEntries;
    pitEntries.clear();
    uint32_t pathNum = pids.size();
    for (uint32_t i = 0; i < pathNum; i++) {
        PathData * curPitEntry = lookup_PIT(pids[i]);
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


void Ipv4SmartFlowRouting::record_reorder_at_dst_tor(Ptr<Packet> &pkt,const Ipv4Header &header){
    if (m_reorderTraceEnable == false) {
        return ;
    }
    
    // NS_LOG_INFO ("############ Fucntion: check_reorder_info_in_dst_switch() ############");
    TcpHeader tcpHeader;
    if (pkt->PeekHeader(tcpHeader) == 0){
        // NS_LOG_INFO("This Packet does not have TCP header");
        return ;
    }
    std::string flowStr = hashing_flow_with_5_tuple_to_string(pkt, header);
    // NS_LOG_INFO("5-tuple Info of the packet :" << flowStr);

    uint32_t tcpHeaderLengthInByte = ((uint32_t)tcpHeader.GetLength())*4;
    uint32_t pktSizeInByte = pkt->GetSize();
    uint32_t tcpSeqNum = tcpHeader.GetSequenceNumber().GetValue();
    // uint32_t expectedSeqNum = tcpSeqNum + pktSizeInByte - tcpHeaderLengthInByte;
    uint32_t tcpPayloadInByte = pktSizeInByte - tcpHeaderLengthInByte;
    std::string flagStr = TcpHeader::FlagsToString (tcpHeader.GetFlags ());

    // NS_LOG_INFO("Packet Size without ipv4 header but With TCP header in byte: " << pktSizeInByte);
    // NS_LOG_INFO("TCP Header Size in Byte: " << tcpHeaderLengthInByte);
    // NS_LOG_INFO("TCP Flags: " << flagStr);
    // NS_LOG_INFO("curSeqNum: " << tcpSeqNum);
    // NS_LOG_INFO("Packet Payload In Byte for TCP Header: " << tcpPayloadInByte);
    // NS_LOG_INFO("expectedSeqNum: " << expectedSeqNum);

    auto it = m_reorderTable.find(flowStr);
    if (flagStr == "SYN"){
        // NS_LOG_INFO("The First handshake packet of the new flow sent from the sender to the receiver");
        reorder_entry_t reorderEntry = reorder_entry_t();
        reorderEntry.flag = true;
        reorderEntry.seqs.clear();
        m_reorderTable[flowStr] = reorderEntry;
    }else if (flagStr == "SYN|ACK") {
        // NS_LOG_INFO("The Second handshake packet of the new flow sent from the receiver to the sender");
        reorder_entry_t reorderEntry = reorder_entry_t();
        reorderEntry.flag = false;
        reorderEntry.seqs.clear();
        m_reorderTable[flowStr] = reorderEntry;
    }else if (flagStr == "FIN|ACK") {
        // NS_LOG_INFO("The teardown packet");
        if (it == m_reorderTable.end()) {
            std::cout << "Error in record_reorder_at_dst_tor() since the un-recorded flow" << std::endl;
        }else if (it->second.flag == false) {
            NS_LOG_INFO("The Second teardown packet from the receiver to the sender");
            // it->second.print();
        }else{
            // NS_LOG_INFO("The First teardown packet with normal payload from the sender to the receiver");
            it->second.seqs.push_back(tcpSeqNum);
            // it->second.print();
        }
    }else if (flagStr == "ACK") {
        // NS_LOG_INFO("The Mid packet of the flow");
        if (it == m_reorderTable.end()) {
            std::cout << "Error in record_reorder_at_dst_tor() since the un-recorded flow" << std::endl;
        }else if (it->second.flag == false) {
            NS_LOG_INFO("From the receiver to the sender");
            // it->second.print();
        }else{
            if ((tcpPayloadInByte == 0)&&(pktSizeInByte==32)&&(tcpSeqNum==1)) {
                NS_LOG_INFO("The third handshake packet of the flow sent from the sender to the receiver");
            }else{
                it->second.seqs.push_back(tcpSeqNum);
            }
            // it->second.print();
        }
    }
    return ;

}

void Ipv4SmartFlowRouting::receive_normal_packet(Ptr<Packet> &pkt, Ipv4SmartFlowPathTag &pathTag, PathData * & pitEntry){
    // NS_LOG_INFO ("############ Fucntion: receive_normal_packet() ############");
    if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
        Ipv4SmartFlowCongaTag congaTag;
        if (pkt->PeekPacketTag(congaTag)) { // reaching the dst ToR switch
            update_PIT_by_conga_tag(congaTag);
        }
        update_PIT_by_path_tag(pathTag, pitEntry);
        return ;

    }
    
    if ((m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY) &&(m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY) ) {
        return ;
    }
    update_PIT_by_path_tag(pathTag, pitEntry);
    update_PIT_by_latency_tag(pkt);
    return ;
  }

void Ipv4SmartFlowRouting::add_probe_tag_by_path_id (Ptr<Packet> packet, uint32_t expiredPathId) {
    // NS_LOG_INFO ("############ Fucntion: add_probe_tag_by_path_id() ############");
    Ipv4SmartFlowProbeTag probeTag = construct_probe_tag_by_path_id(expiredPathId);
    packet->AddPacketTag(probeTag);
    return;
}

void Ipv4SmartFlowRouting::update_PIT_after_probing (PathData * pitEntry) {
    // NS_LOG_INFO ("############ Fucntion: update_PIT_after_probing() ############");
    pitEntry->tsProbeLastSend = Simulator::Now();
    return ;
}


void Ipv4SmartFlowRouting::add_path_tag_by_path_id (Ptr<Packet> packet, uint32_t pid) {
    // NS_LOG_INFO ("############ Fucntion: add_path_tag_by_path_id() ############");
    Ipv4SmartFlowPathTag pathTag = construct_path_tag(pid);
    packet->AddPacketTag(pathTag);
    return ;
}

uint32_t Ipv4SmartFlowRouting::get_the_path_length_by_path_id(const uint32_t pathId, PathData * & pitEntry){
    // NS_LOG_INFO ("############ Fucntion: get_the_path_length_by_path_id() ############");
    pitEntry = lookup_PIT(pathId);
    return pitEntry->portSequence.size();
}

bool Ipv4SmartFlowRouting::reach_the_last_hop_of_path_tag(Ipv4SmartFlowPathTag & smartFlowTag, PathData * & pitEntry){
    // NS_LOG_INFO ("############ Fucntion: reach_the_last_hop_of_path_tag() ############");
    uint32_t pathId = smartFlowTag.get_path_id();
    uint32_t pathSize = get_the_path_length_by_path_id(pathId, pitEntry);
    uint32_t hopIdx = smartFlowTag.get_the_current_hop_index();
    if (hopIdx == pathSize) {
        // NS_LOG_INFO ("Reaching the Dst ToR");
        return true;
    }else{
        // NS_LOG_INFO ("Is " << hopIdx << "/" << pathSize << " hops");
        return false;
    }
}

bool Ipv4SmartFlowRouting::exist_path_tag(Ptr<Packet> packet, Ipv4SmartFlowPathTag & pathTag){
    // // NS_LOG_INFO ("############ Fucntion: exist_path_tag() ############");
    if (packet->PeekPacketTag(pathTag)) { // reaching the src ToR switch
        // NS_LOG_INFO ("Path Tag indeed exits");
        return true;
    }
    // NS_LOG_INFO ("Path Tag does not exit");
    return false;
}

bool Ipv4SmartFlowRouting::exist_probe_tag(Ptr<Packet> packet, Ipv4SmartFlowProbeTag & probeTag){
    // NS_LOG_INFO ("############ Fucntion: exist_probe_tag() ############");
    if (packet->PeekPacketTag(probeTag)) { // reaching the src ToR switch
        // NS_LOG_INFO ("Probe Tag indeed exits");
        return true;
    }
    // NS_LOG_INFO ("Probe Tag does not exit");
    return false;
}

uint32_t Ipv4SmartFlowRouting::get_egress_port_id_by_path_tag(Ipv4SmartFlowPathTag & smartFlowTag){
    // NS_LOG_INFO ("############ Fucntion: get_egress_port_id_by_path_tag() ############");
    uint32_t pathId = smartFlowTag.get_path_id();
    uint32_t hopIdx = smartFlowTag.get_the_current_hop_index();
    PathData * pitEntry = lookup_PIT(pathId);
    return pitEntry->portSequence[hopIdx];
}

std::vector<PathData *> Ipv4SmartFlowRouting::get_the_piggyback_pit_entries(Ipv4Address srcToRAddr, Ipv4Address dstToRAddr){
    // NS_LOG_INFO ("############ Fucntion: get_the_piggyback_pit_entries() ############");
    PathSelTblKey pstKey(srcToRAddr, dstToRAddr);
    pstEntryData* pstEntry = lookup_PST(pstKey);
    std::vector<PathData *> allPitEntries = batch_lookup_PIT(pstEntry->paths);
    std::vector<PathData *> freshPitEntries = get_the_newly_measured_paths(allPitEntries);
    std::vector<PathData *> piggybackPitEntries = get_the_best_piggyback_paths(freshPitEntries);
    return piggybackPitEntries;
}

uint32_t Ipv4SmartFlowRouting::QuantizingX (uint32_t X) {
  DataRate c = m_C;
  double ratio = static_cast<double> (X * 8) / (c.GetBitRate () * m_tdre.GetSeconds () / m_alpha);
  uint32_t t = static_cast<uint32_t>(ratio * std::pow(2, m_Q));
  NS_LOG_INFO ("x: " << X << ", ratio: " << ratio << ", dreVal: " << t);
  return t;
}



PathData * Ipv4SmartFlowRouting::get_the_piggyback_pit_entry_for_conga(Ipv4Address srcToRAddr, Ipv4Address dstToRAddr){
    // std::cout << "Enter get_the_piggyback_pit_entry_for_conga()"<<std::endl;
     PathData * piggybackPitEntry = 0;
    PathSelTblKey pstKey(srcToRAddr, dstToRAddr);
    pstEntryData* pstEntry = lookup_PST(pstKey);
    for (uint32_t i = 1; i <= pstEntry->pathNum; i++) {
        uint32_t tmpPathIdx = (pstEntry->lastPiggybackPathIdx+i)%pstEntry->pathNum;
        uint32_t tmpPathId = pstEntry->paths[tmpPathIdx];
        PathData * tmpPitEntry = lookup_PIT(tmpPathId);
        if (tmpPitEntry->tsGeneration > tmpPitEntry->tsLatencyLastSend) {
            NS_LOG_INFO("Node "<<m_nodeId << " piggyback the following path");
            // pstEntry->print();
            // tmpPitEntry->print();
            piggybackPitEntry = tmpPitEntry;
            piggybackPitEntry->tsLatencyLastSend = Simulator::Now();
            pstEntry->lastPiggybackPathIdx = tmpPathIdx;
            // pstEntry->print();
            // piggybackPitEntry->print();
            return piggybackPitEntry;
        }
    }
    NS_LOG_INFO("Node "<<m_nodeId << " no need to piggyback");

    return piggybackPitEntry;
}

void Ipv4SmartFlowRouting::PrintDreTable () {

  std::ostringstream oss;
  oss << "==== Local Dre for Node " << m_nodeId << " ====" <<std::endl;
  std::map<uint32_t, uint32_t>::iterator itr = m_XMap.begin ();
  for ( ; itr != m_XMap.end (); ++itr)
  {
    oss << "port: " << itr->first << ", X: " << itr->second << ", Quantized X: " << QuantizingX (itr->second) <<std::endl;
  }
  oss << "=================================";
  NS_LOG_LOGIC (oss.str ());
}


void Ipv4SmartFlowRouting::DreEvent () {
  NS_LOG_INFO (Simulator::Now ().GetMicroSeconds() << "us, Port Dre event Runs at Node "<<m_nodeId);
//   std::cout << "Node " << m_nodeId << ", " << Simulator::Now ().GetMicroSeconds() << "us, runs local Dre event"<<std::endl;

  bool moveToIdleStatus = true;

  std::map<uint32_t, uint32_t>::iterator itr = m_XMap.begin ();
  for ( ; itr != m_XMap.end (); ++itr ) {
    uint32_t newX = itr->second * (1 - m_alpha);
    // if ((itr->second != 0) || (newX != 0)) {
    //     NS_LOG_INFO("Port " << itr->first << " dre from " << itr->second << " to "<<newX);
    //     // std::cout <<"Port " << itr->first << " dre from " << itr->second << " to "<<newX << std::endl;
    // }
    itr->second = newX;

    
    if (newX != 0)  {
      moveToIdleStatus = false;
    }
  }

//   NS_LOG_LOGIC (this << " Dre event finished, the dre table is now: ");
//   PrintDreTable ();

  if (!moveToIdleStatus)
  {
    m_dreEvent = Simulator::Schedule(m_tdre, &Ipv4SmartFlowRouting::DreEvent, this);
    // std::cout << "Next, Node " << m_nodeId << ", " << Simulator::Now ().GetMicroSeconds() + m_tdre.GetMicroSeconds()<< "us, runs local Dre event"<<std::endl;

  }
//   else
//   {
//     NS_LOG_LOGIC (this << " Dre event goes into idle status");
//   }
}

std::vector<uint32_t> Ipv4SmartFlowRouting::get_dre_of_egress_ports(std::vector<uint32_t>& ports){
    uint32_t n = ports.size();
    std::vector<uint32_t> dres(n);
    for (size_t i = 0; i < n; i++) {
        auto it = m_XMap.find(ports[i]);
        if (it == m_XMap.end()) {
            std::cout << "Error in get_dre_of_egress_ports() with invalid portId: " << ports[i] << std::endl;
            PrintDreTable();
            return dres;
        }else{
            uint32_t dre = QuantizingX(it->second);
            dres[i] = dre;
        }
    }
    return dres;
  }

void Ipv4SmartFlowRouting::AgingEvent () {
    NS_LOG_INFO (Simulator::Now ().GetMicroSeconds() << "us, Path Age event Runs at Node "<<m_nodeId);
//   std::cout << "Node " << m_nodeId << ", " << Simulator::Now ().GetMicroSeconds() << "us, runs remote Dre event"<<std::endl;

    bool moveToIdleStatus = true;
    for (auto & it:  m_nexthopSelTbl){
        if (it.second.nodeIdSequence[0] == m_nodeId) { // forward path
            // NS_LOG_INFO ("Node " << m_nodeId << " AgingEvent goes into aging for forwarding paths");
            // it.second.print();
            if (Simulator::Now () - (it.second).tsGeneration > m_agingTime)  {
                // std::cout <<  "Node " << m_nodeId << " reset the following path "<< it.second.pid << std::endl;
                // it.second.print();
                // if (it.second.pathDre != 0) {
                //     NS_LOG_INFO("Path " << it.second.pid << " dre from " << it.second.pathDre << " to "<<0);
                //     // std::cout << "Path " << it.second.pid << " dre from " << it.second.pathDre << " to "<<0<<std::endl;

                // }
                

                it.second.pathDre = 0;
            }else {
                moveToIdleStatus = false;
            }
        }
    }
    if (!moveToIdleStatus) {
        NS_LOG_LOGIC (this << " Aging event runs again after" << m_agingTime / 4);
        m_agingEvent = Simulator::Schedule(m_agingTime / 4, &Ipv4SmartFlowRouting::AgingEvent, this);
        // std::cout << "Next, Node " << m_nodeId << ", " << Simulator::Now ().GetMicroSeconds() + (m_agingTime / 4).GetMicroSeconds()<< "us, runs remote Dre event"<<std::endl;

    }
    // else {
    //     NS_LOG_LOGIC (this << " Aging event goes into idle status");
    // }
}

uint32_t Ipv4SmartFlowRouting::UpdateLocalDre (const Ipv4Header &header, Ptr<Packet> packet, uint32_t port) {
  uint32_t X = 0;
  std::map<uint32_t, uint32_t>::iterator XItr = m_XMap.find(port);
  if (XItr != m_XMap.end ()) {
    X = XItr->second;
  }else{
    std::cout << "Error in UpdateLocalDre() with out a valid entry in m_XMap" << std::endl;
  }
  uint32_t newX = X + packet->GetSize () + header.GetSerializedSize ();
  NS_LOG_INFO("Node " << m_nodeId << " Updates local Dre on Port " << port << " from " << X << " to " << newX);
//   NS_LOG_LOGIC (this << " Update local dre, new X: " << newX);
  m_XMap[port] = newX;
  return newX;
}


uint32_t Ipv4SmartFlowRouting::forward_normal_packet(Ptr<Packet> &pkt,
                                               const Ipv4Header &header,
                                               UnicastForwardCallback ucb,
                                               Ipv4Address srcToRAddr,
                                               Ipv4Address dstToRAddr,
                                               Ipv4Address dstServerAddr){
    // NS_LOG_INFO("PST strategy=" << m_pathSelStrategy << "<1:min, 2:rbnp, 3:rnd, 4:lspray, 5:flet, 6:ecmp, 7:rbnf, 8:rbnh>");
    // ############ Function: forward_normal_packet() ############");


    if (m_pathSelStrategy == PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY ) {
        std::vector<PathData *> forwardPitEntries;
        PathSelTblKey forwarPstKey(srcToRAddr, dstToRAddr);
        pstEntryData* forwardPstEntry = lookup_PST(forwarPstKey);
        NS_LOG_INFO("forward PST Entry");
        // forwardPstEntry->print();

        uint32_t forwardPathNum =  forwardPstEntry->pathNum;
        NS_LOG_INFO("m_pathSelNum, forwardPathNum=" << m_pathSelNum << ", " << forwardPathNum);
        if (m_pathSelNum >= forwardPathNum) {
            NS_LOG_INFO("There is too Less available forwarding paths");
            forwardPitEntries = batch_lookup_PIT(forwardPstEntry->paths);
        }else{
            NS_LOG_INFO("There is too Many available forwarding paths, so to select " << m_pathSelNum << " paths");
            for (uint32_t i = 0; i < m_pathSelNum; i++){
                uint32_t rndPathIdx = std::rand()%forwardPathNum;
                // std::cout << "The " << i << "-th path index is " << rndPathIdx << std::endl;
                uint32_t rndPathId = forwardPstEntry->paths[rndPathIdx];
                PathData * rndPitEntry = lookup_PIT(rndPathId);
                forwardPitEntries.push_back(rndPitEntry);
            }
        }
        NS_LOG_INFO("forward PIT Entries");
        // for (auto e : forwardPitEntries) {
        //     e->print();
        // }

        forward_probe_packet_optimized(pkt, forwardPitEntries, header, ucb, dstServerAddr); // probing

        PathSelTblKey reversePstKey(dstToRAddr, srcToRAddr);                               
        pstEntryData* reversePstEntry = lookup_PST(reversePstKey);
        NS_LOG_INFO("reverse PST Entry");
        // reversePstEntry->print();
        uint32_t reversePathNum = reversePstEntry->pathNum;
        NS_LOG_INFO("m_piggyLatencyCnt, reversePathNum=" << m_piggyLatencyCnt << ", " << reversePathNum);

        std::vector<PathData *> reversePitEntries;
        if (m_piggyLatencyCnt >= reversePathNum) {
            NS_LOG_INFO("There is too Less available reverse paths");
            reversePitEntries = batch_lookup_PIT(reversePstEntry->paths);
        }else{
            NS_LOG_INFO("There is too Many available reverse paths, so to select " << m_piggyLatencyCnt << " paths");
            for (uint32_t i = 0; i < m_piggyLatencyCnt; i++){
                uint32_t rndPathIdx = std::rand()%reversePathNum;
                // std::cout << "The " << i << "-th path index is " << rndPathIdx << std::endl;
                uint32_t rndPathId = reversePstEntry->paths[rndPathIdx];
                PathData * rndPitEntry = lookup_PIT(rndPathId);
                reversePitEntries.push_back(rndPitEntry);
            }
        }
        NS_LOG_INFO("reverse PIT Entries");
        // for (auto e : reversePitEntries) {
        //     e->print();
        // }
        std::vector<PathData *> freshrPitEntries = get_the_newly_measured_paths(reversePitEntries);
        NS_LOG_INFO("piggyback PIT Entries");
        // for (auto e : freshrPitEntries) {
        //     e->print();
        // }
        add_latency_tag_by_pit_entries (pkt, freshrPitEntries);
        update_PIT_after_piggybacking(freshrPitEntries);                                  // piggybacking
        NS_LOG_INFO("PIT Entries after piggybacking");
        // for (auto e : freshrPitEntries) {
        //     e->print();
        // }
        std::sort(forwardPitEntries.begin(), forwardPitEntries.end(), cmp_pitEntry_in_increase_order_of_latency);
        PathData * curBestForwardPitEntry = forwardPitEntries[0];
        NS_LOG_INFO("The cur Best Forward Pit Entry");
        // curBestForwardPitEntry->print();

        pdt_entry_t * bestForwadPitEntry = lookup_PDT(forwarPstKey);
        if (bestForwadPitEntry == 0) {
            // std::cout << "The new flow, to build a PDT entry" << std::endl;
            pdt_entry_t pdtEntry = pdt_entry_t();
            // pdtEntry.pid = curBestForwardPitEntry->pid;
            // pdtEntry.latency = curBestForwardPitEntry->latency;
            pdtEntry.pit = curBestForwardPitEntry;
            m_pathDecTbl[forwarPstKey] = pdtEntry;
            // m_pathDecTbl[forwarPstKey].print();
            bestForwadPitEntry = &m_pathDecTbl[forwarPstKey];
        }else if (curBestForwardPitEntry->latency < bestForwadPitEntry->pit->latency) {
            NS_LOG_INFO("The exist flow, the better Pid, to update pid and latency in PDT entry");
            // bestForwadPitEntry->print();
            bestForwadPitEntry->pit = curBestForwardPitEntry;
            // bestForwadPitEntry->latency = curBestForwardPitEntry->latency;
            // bestForwadPitEntry->print();
        }else{
            NS_LOG_INFO("The exist flow, the worse Pid, use the previous PDT entry");
        }
        uint32_t bestForwardPid = bestForwadPitEntry->pit->pid;
        NS_LOG_INFO("The final forward path Id");
        // std::cout << "bestForwardPid: "<< bestForwardPid << std::endl;
        add_path_tag_by_path_id(pkt, bestForwardPid);
        output_packet_by_path_tag(pkt, header, ucb, dstServerAddr); // sending
        return pkt->GetSize();
    }

    std::vector<PathData *> pitEntries;
    PathData *  bestPitEntry = 0;
    PathSelTblKey pstKey(srcToRAddr, dstToRAddr);
    pstEntryData* pstEntry = lookup_PST(pstKey);

    pitEntries = batch_lookup_PIT(pstEntry->paths);
    if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
        // Turn on DRE event scheduler if it is not running

        bestPitEntry = get_the_best_forwarding_path(pitEntries, pstEntry, pkt, header);
        Ipv4SmartFlowPathTag pathTag = construct_path_tag(bestPitEntry->pid);
        pkt->AddPacketTag(pathTag);

        PathData * pBPitEntry = get_the_piggyback_pit_entry_for_conga(dstToRAddr, srcToRAddr);
        add_conga_tag_by_pit_entry (pkt, pBPitEntry);
        output_packet_by_path_tag(pkt, header, ucb, dstServerAddr); // normal pkt
        return pkt->GetSize();
    }
    bestPitEntry = get_the_best_forwarding_path(pitEntries, pstEntry, pkt, header);
    add_path_tag_by_path_id(pkt, bestPitEntry->pid);
    update_PIT_after_adding_path_tag(bestPitEntry);
    if (m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY) {
        output_packet_by_path_tag(pkt, header, ucb, dstServerAddr); // normal pkt
        return pkt->GetSize();
    }
    std::vector<PathData *> piggyBackPitEntries = get_the_piggyback_pit_entries(dstToRAddr, srcToRAddr);
    add_latency_tag_by_pit_entries (pkt, piggyBackPitEntries);
    update_PIT_after_piggybacking(piggyBackPitEntries);
    output_packet_by_path_tag(pkt, header, ucb, dstServerAddr); // normal pkt
    forward_probe_packet(pkt, pitEntries, bestPitEntry, header, ucb, dstServerAddr);
    return pkt->GetSize();
  }

uint32_t Ipv4SmartFlowRouting::forward_probe_packet(Ptr<Packet> pkt,
                                               std::vector<PathData *> &forwardPitEntries,
                                               PathData * bestForwardingPitEntry,
                                               const Ipv4Header &header,
                                               UnicastForwardCallback ucb,
                                               Ipv4Address dstServerAddr){
    if (m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY) {
        return 0;
    }
    std::vector<PathData *> expiredPitEntries, probePitEntries, potentialPitEntries;
    get_the_expired_paths(forwardPitEntries, expiredPitEntries);
    get_the_probe_paths (expiredPitEntries, probePitEntries);
    get_the_potential_paths (bestForwardingPitEntry, probePitEntries, potentialPitEntries);
    PathData * bestProbingPitEntry = get_the_best_probing_path (potentialPitEntries);
    if (bestProbingPitEntry == 0) {
        return 0;
    }
    Ptr<Packet> probePacket = construct_probe_packet(pkt);
    add_path_tag_by_path_id(probePacket, bestProbingPitEntry->pid);
    add_probe_tag_by_path_id(probePacket, bestProbingPitEntry->pid);
    output_packet_by_path_tag(probePacket, header, ucb, dstServerAddr);
    update_PIT_after_probing (bestProbingPitEntry);
    record_the_probing_info(bestProbingPitEntry->pid);
    return probePacket->GetSize();
  }

uint32_t Ipv4SmartFlowRouting::forward_probe_packet_optimized(Ptr<Packet> pkt,
                                               std::vector<PathData *> &forwardPitEntries,
                                               const Ipv4Header &header,
                                               UnicastForwardCallback ucb,
                                               Ipv4Address dstServerAddr){
    if (m_pathSelStrategy != PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY) {
        return 0;
    }
    std::vector<PathData *> expiredPitEntries, probePitEntries, potentialPitEntries;
    get_the_expired_paths(forwardPitEntries, expiredPitEntries);
    get_the_probe_paths (expiredPitEntries, probePitEntries);
    PathData * bestProbingPitEntry = get_the_best_probing_path (probePitEntries);    
    if (bestProbingPitEntry == 0) {
        // std::cout << "No need to probe" << std::endl;
        return 0;
    }
    // std::cout << "The probe path" << std::endl;
    // bestProbingPitEntry->print();

    Ptr<Packet> probePacket = construct_probe_packet(pkt);
    add_path_tag_by_path_id(probePacket, bestProbingPitEntry->pid);
    add_probe_tag_by_path_id(probePacket, bestProbingPitEntry->pid);
    output_packet_by_path_tag(probePacket, header, ucb, dstServerAddr);
    update_PIT_after_probing (bestProbingPitEntry);
    // bestProbingPitEntry->print();
    record_the_probing_info(bestProbingPitEntry->pid);
    return probePacket->GetSize();
  }

void Ipv4SmartFlowRouting::update_path_dre_for_conga_at_mid_sw(Ptr<Packet> pkt){
    NS_LOG_INFO ("############ Fucntion: update_path_dre_for_conga_at_mid_sw() ############");
    Ipv4SmartFlowPathTag pathTag;
    if (pkt->PeekPacketTag(pathTag)) {
        uint32_t egressPort = get_egress_port_id_by_path_tag(pathTag);
        auto it = m_XMap.find(egressPort);
        if (it == m_XMap.end()) {
            std::cout << "Error in update_path_dre_for_conga_at_mid_sw() with invalid portId: " << egressPort << std::endl;
            PrintDreTable();
            return ;
        }else{
            uint32_t preDre = pathTag.GetDre();
            uint32_t curDre = QuantizingX(it->second);
            uint32_t maxDre = std::max (preDre, curDre);
            pathTag.SetDre(maxDre);
            // uint32_t x = std::rand()%8;
            // pathTag.SetDre(x);
            // std::cout<<"Node "<<m_nodeId<<" update path " <<pathTag.get_path_id()<< " Dre from "<<preDre<<" to "<<x<<std::endl;

            NS_LOG_INFO ("Node "<<m_nodeId<<" update path Dre from "<<preDre<<" to "<<maxDre);
            pkt->ReplacePacketTag (pathTag);
        }
        return ;
    }
}

bool Ipv4SmartFlowRouting::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, 
    UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb) {
    NS_LOG_INFO ("########## Node: "<< get_node_id() << ", Time: "<< Simulator::Now().GetMicroSeconds() << "us, Size:" << p->GetSize() <<" ################## Function: RouteInput() #######################");
    // std::cout << "########## Node: "<< get_node_id() << ", Time: "<< Simulator::Now().GetMicroSeconds() << "us, PktSize:" << p->GetSize() << std::endl;

    if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY) {
        if (!m_dreEvent.IsRunning ()) {
            NS_LOG_INFO ("Node "<<m_nodeId << " Conga routing restarts local dre event scheduling, locally");
            m_dreEvent = Simulator::Schedule(m_tdre, &Ipv4SmartFlowRouting::DreEvent, this);
            // std::cout << "Node "<<m_nodeId << " is going to run local dre event at "<<Simulator::Now().GetMicroSeconds()+m_tdre.GetMicroSeconds() << " us" << std::endl;

        }
        if (!m_agingEvent.IsRunning ()) {
            NS_LOG_INFO ("Node "<<m_nodeId << " Conga routing restarts global dre event scheduling, globally");
            m_agingEvent = Simulator::Schedule(m_agingTime / 4, &Ipv4SmartFlowRouting::AgingEvent, this);
            // std::cout << "Node "<<m_nodeId << " is going to run remote dre event at "<<Simulator::Now().GetMicroSeconds()+(m_agingTime / 4).GetMicroSeconds() << " us" << std::endl;

        }    
    }
    
    // NS_LOG_INFO ("Arriving Packet: size=" << p->GetSize());
    NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev << &lcb << &ecb);
    NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
    Ipv4Address dstServerAddr = header.GetDestination();
    Ipv4Address srcServerAddr = header.GetSource();
    // NS_LOG_INFO ("(srcServer, dstServer)=("<< srcServerAddr<<", "<<dstServerAddr<<")");

    // 路由仅支持单播
    if (dstServerAddr.IsMulticast() || dstServerAddr.IsBroadcast()) {
        return false;
    }
    // 检查输入设备是否支持 IP 转发
    uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);
    if (m_ipv4->IsForwarding(iif) == false) {
      return false;
    }

    // NS_LOG_INFO("Time " << Simulator::Now() << ", ");
    // NS_LOG_INFO("Node " << get_node_id() << ", ");
    // NS_LOG_INFO("srcServerAddr " << srcServerAddr << ", ");
    // NS_LOG_INFO("dstServerAddr " << dstServerAddr << ", ");

    Ipv4Address srcToRAddr = lookup_VMT(srcServerAddr)->torAddr;
    Ipv4Address dstToRAddr = lookup_VMT(dstServerAddr)->torAddr;
    // NS_LOG_INFO ("(pktSize, srcServer, dstServer, srcToRAddr, dstToRAddr)=(" << p->GetSize() << ", "<< srcServerAddr << ", " << dstServerAddr << ", " << srcToRAddr <<", " << dstToRAddr << ")");
    // NS_LOG_INFO("srcToRAddr " << srcToRAddr << ", ");
    // NS_LOG_INFO("dstToRAddr " << dstToRAddr << ", ");
    // NS_LOG_INFO("\n");
    Ptr<Packet> packet = ConstCast<Packet>(p);
    Ipv4SmartFlowPathTag pathTag;
    Ipv4SmartFlowProbeTag probeTag;

    // bool existPathTag = exist_path_tag(packet, pathTag);
    // NS_LOG_INFO("existPathTag " << existPathTag);
    if (exist_path_tag(packet, pathTag) == false) { // reaching the src ToR Switch
        // NS_LOG_FUNCTION ("Reaching the Source ToR switch");
        if (srcToRAddr == dstToRAddr) { // communicate within the same ToR 
            NS_LOG_INFO ("Intra Switch : (srcServer, dstServer)="<< srcServerAddr<<", "<<dstServerAddr<<")");
            return false;
        }else{
            forward_normal_packet(packet, header, ucb, srcToRAddr, dstToRAddr, dstServerAddr);
            // forward_probe_packet(packet, forwardPitEntries, bestForwardingPitEntry, header, ucb, dstServerAddr);
            // NS_LOG_INFO("FINISH the RouteInput");
            return true;
        }
    }else{
        PathData * pitEntry;
        if(reach_the_last_hop_of_path_tag(pathTag, pitEntry)==true){ // dst ToR
            NS_LOG_FUNCTION ("Reaching the Destination ToR switch");
            if (exist_probe_tag(packet, probeTag)==true){ // probe pkt
                NS_LOG_FUNCTION ("Is a Probe Packet");
                receive_probe_packet(probeTag);
                NS_LOG_INFO("FINISH the RouteInput, has probe tag");
                return true;
            }else{ // normal pkt
                // NS_LOG_FUNCTION ("Is a Normal Packet");
                receive_normal_packet(packet, pathTag, pitEntry);
                if (m_reorderFlag  != 0) {
                    record_reorder_at_dst_tor(packet, header);
                }
                // NS_LOG_INFO("FINISH the RouteInput, normal packet, end");
                return false;
            }
        }else{ // mid ToR
            // NS_LOG_FUNCTION ("Reaching the Intermediate switch");
            if (m_pathSelStrategy == PATH_SELECTION_CONGA_STRATEGY){
                update_path_dre_for_conga_at_mid_sw(packet);
            }
            output_packet_by_path_tag(packet, header, ucb, dstServerAddr); // normal pkt
                                                        // NS_LOG_INFO("FINISH the RouteInput, normal packet, mid");

            return true;
        }

    }
}

void Ipv4SmartFlowRouting::NotifyInterfaceUp (uint32_t interface)
{
    return;
}

void Ipv4SmartFlowRouting::NotifyInterfaceDown (uint32_t interface)
{
    return;
}

void Ipv4SmartFlowRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
    return;
}

void Ipv4SmartFlowRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
    return;
}

void Ipv4SmartFlowRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
    NS_LOG_LOGIC (this << "Setting up Ipv4: " << ipv4);
    NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
    m_ipv4 = ipv4;
    return;
}

void Ipv4SmartFlowRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
   std::cout << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
       << ", Ipv4SmartFlowRouting table" << std::endl;
    
    for (std::map<uint32_t, PathData>::const_iterator it = m_nexthopSelTbl.begin(); it != m_nexthopSelTbl.end(); it++) {
        std::cout << "Path Id: " << it->first << std::endl;
        std::cout << "port sequence: ";
        uint32_t size = it->second.portSequence.size();
        for (uint32_t i = 0; i < size; i++) {
            std::cout << " " <<  it->second.portSequence[i];
        }
        std::cout << "latency: " << it->second.latency << std::endl;
    }
    return;
}

void Ipv4SmartFlowRouting::set_PST(std::map<PathSelTblKey, pstEntryData> &pathSelTbl)
{
    m_pathSelTbl.insert(pathSelTbl.begin(), pathSelTbl.end());
    //NS_LOG_INFO ("m_pathSelTbl size " << m_pathSelTbl.size());
    return;
}

std::map<PathSelTblKey, pstEntryData> Ipv4SmartFlowRouting::get_PST() const
{
    return m_pathSelTbl;
}

void Ipv4SmartFlowRouting::set_PIT(std::map<uint32_t, PathData> &nexthopSelTbl)
{
    m_nexthopSelTbl.insert(nexthopSelTbl.begin(), nexthopSelTbl.end());
    //NS_LOG_INFO ("m_nexthopSelTbl size " << m_nexthopSelTbl.size());
    return;
}

std::map<uint32_t, PathData> Ipv4SmartFlowRouting::get_PIT() const
{
    return m_nexthopSelTbl;
}

void Ipv4SmartFlowRouting::set_VMT(std::map<Ipv4Address, vmt_entry_t> &vmVtepMapTbl)
{
    m_vmVtepMapTbl.insert(vmVtepMapTbl.begin(), vmVtepMapTbl.end());
    //NS_LOG_INFO ("m_vmVtepMapTbl size " << m_vmVtepMapTbl.size());
    return;
}

std::map<Ipv4Address, vmt_entry_t> Ipv4SmartFlowRouting::get_VMT() const {
    return m_vmVtepMapTbl;
}

void Ipv4SmartFlowRouting::set_probing_interval(uint32_t probeTimeInterval) {
    m_probeTimeInterval = probeTimeInterval;
    return;
}

uint32_t Ipv4SmartFlowRouting::get_probing_interval() const {
    return m_probeTimeInterval;
}

void Ipv4SmartFlowRouting::set_path_expire_interval(uint32_t a)
{
    m_pathExpiredTimeThld = a;
    return;
}
uint32_t Ipv4SmartFlowRouting::get_path_expire_interval() const
{
    return m_pathExpiredTimeThld;
}

void Ipv4SmartFlowRouting::set_max_piggyback_path_number(uint32_t piggyLatencyCnt)
{
    m_piggyLatencyCnt = piggyLatencyCnt;
    return;
}

uint32_t Ipv4SmartFlowRouting::get_max_piggyback_path_number() const
{
    return m_piggyLatencyCnt;
}





void Ipv4SmartFlowRouting::record_the_probing_info(uint32_t pathId){
    if (m_reorderTraceEnable == false) {
        return ;
    }
    
    if (pathId >= m_prbInfoTable.size()) {
        m_prbInfoTable.resize(pathId+1);
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