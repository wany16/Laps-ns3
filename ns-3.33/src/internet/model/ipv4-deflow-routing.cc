/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ipv4-deflow-routing.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE("Ipv4DeflowRouting");

NS_OBJECT_ENSURE_REGISTERED (Ipv4DeflowRouting);
// void Ipv4DeflowRouting::PrintQueueLen (std::string 	context, uint32_t oldValue, uint32_t newValue)
// {
//   NS_LOG_INFO (Simulator::Now () << context << "'s old and new Queue Lens are "
//   << oldValue << "  " << newValue);
// }

std::map<uint16_t, uint32_t> Ipv4DeflowRouting::MapOfport2flowSize;

std::string GetNodeName(const Ptr<Node> node, bool withIndex){
  if (node == 0) {
    std::cout << "Error with zero Node pointer" << std::endl;
  }
  std::string nodeName;
  if (withIndex) {
    nodeName = Names::FindName (node) != "" ? Names::FindName (node) : "UNKNOWN";
  }else{
    nodeName = Names::FindName (node) != ""
                              ? Names::FindName (node)+"("+std::to_string(node->GetId()) +")"
                              : "UNKNOWN("+std::to_string(node->GetId())+")";
  }
  

  return nodeName;
}

std::string Ipv4Address2String(Ipv4Address addr) {
  std::ostringstream os;
  addr.Print(os);
  std::string addrStr = os.str();
  return addrStr;
}

uint32_t Ipv4DeflowRouting::GetQueueLength(const uint32_t egressPortIdx) const {
  Ptr<Ipv4L3Protocol> ipv4L3Protocol = DynamicCast<Ipv4L3Protocol> (m_ipv4);
  if (!ipv4L3Protocol)  {
    NS_LOG_ERROR (this << " Error in GetQueueLength(): cannot work other than Ipv4L3Protocol");
    return 0;
  }
  uint32_t qlenInByte = 0;
  uint32_t devNum = m_ipv4->GetNInterfaces ();
  if ((egressPortIdx >= devNum) || (egressPortIdx == 0) ) {
      std::cout << " Error in GetQueueLength(): wrong egressPortIdx index = " << egressPortIdx << std::endl;
      return 0;
  }
  
  const Ptr<NetDevice> dev = this->m_ipv4->GetNetDevice (egressPortIdx);
  if (dev->IsPointToPoint ()) {
    Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice> (dev);
    if (p2pDev) {
          qlenInByte += p2pDev->GetQueue()->GetNBytes();
    }else{
      std::cout << " Error in GetQueueLength(): wrong device type" << std::endl;
    }
  }else{
    std::cout << " Error in GetQueueLength(): wrong device type" << std::endl;
  }
  return qlenInByte;
}

Ipv4DeflowRouting::Ipv4DeflowRouting () {
  // std::cout << "Ipv4DeflowRouting::Ipv4DeflowRouting ()" << std::endl;
  NS_LOG_FUNCTION (this);
  m_ipv4 = 0;
  m_routeTable.clear();
  m_drillTable.clear();
  m_packetGapThreshInNs = 1000*1000;
  m_packetGapExpandFactor = 2;
  m_packetGapAgingFactor = 0.5;

  m_packetSizeThreshInByte = 1200;
  m_packetSizeShrinkFactor = 0.5;
  m_packetSizeAgingFactor = 0.5;
  m_congestionThreshInByte = 1500*10;

  m_loadbalancebAlgorithm = LoadBalancingAlgorithm::NONE;

}

Ipv4DeflowRouting::~Ipv4DeflowRouting () {
  NS_LOG_FUNCTION (this);
}

TypeId Ipv4DeflowRouting::GetTypeId (void) {
  static TypeId tid = TypeId("ns3::Ipv4DeflowRouting")
      .SetParent<Ipv4RoutingProtocol>()
      .SetGroupName ("Internet")
      .AddConstructor<Ipv4DeflowRouting> ()
      .AddAttribute ("flowletTimoutInUs", "The default flowlet timeout in microsecond",
                   UintegerValue (100),
                   MakeUintegerAccessor (&Ipv4DeflowRouting::m_flowletTimoutInUs),
                   MakeUintegerChecker<uint64_t> ())
      .AddAttribute ("congestionThreshInByte", " The threshold for link congestion",
                   DoubleValue (1500*10),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_congestionThreshInByte),
                   MakeDoubleChecker<double> ())
      .AddAttribute ("heavyCongestionThreshInByte", " The threshold for heavy link congestion",
                   DoubleValue (1500*30),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_heavyCongestionThreshInByte),
                   MakeDoubleChecker<double> ())
      .AddAttribute ("packetGapThreshInNs", " The threshold for packet gap",
                   DoubleValue (1000*10),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_packetGapThreshInNs),
                   MakeDoubleChecker<double> ())
      .AddAttribute ("packetSizeThreshInByte", " The threshold for packet size",
                   DoubleValue (1200),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_packetSizeThreshInByte),
                   MakeDoubleChecker<double> (0, 1500))
      .AddAttribute ("packetGapExpandFactor", " The initial value for packet gap",
                   DoubleValue (2),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_packetGapExpandFactor),
                   MakeDoubleChecker<double> (1, 10))
      .AddAttribute ("packetSizeShrinkFactor", " The initial value for packet size",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_packetSizeShrinkFactor),
                   MakeDoubleChecker<double> (0, 1))
      .AddAttribute ("packetGapAgingFactor", " The aging rate for packet gap",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_packetGapAgingFactor),
                   MakeDoubleChecker<double> (0, 1))
      .AddAttribute ("packetSizeAgingFactor", " The aging rate for packet size",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&Ipv4DeflowRouting::m_packetSizeAgingFactor),
                   MakeDoubleChecker<double> (0, 1))
      .AddAttribute ("lbs", "Load balancing algorithm.",
                   EnumValue (LoadBalancingAlgorithm::ECMP),
                   MakeEnumAccessor (&Ipv4DeflowRouting::m_loadbalancebAlgorithm),
                   MakeEnumChecker (LoadBalancingAlgorithm::ECMP, "ECMP",
                                    LoadBalancingAlgorithm::DRILL, "DRILL",
                                    LoadBalancingAlgorithm::LETFLOW, "LETFLOW",
                                    LoadBalancingAlgorithm::DEFLOW, "DEFLOW",
                                    LoadBalancingAlgorithm::RPS, "RPS"))

      .AddTraceSource ("QueueLength",
                     "The number of bytes in the queue.",
                     MakeTraceSourceAccessor (&Ipv4DeflowRouting::m_qlen),
                     "ns3::TracedValueCallback::Uint32")
      // .AddTraceSource ("flowClassification",
      //               "The attribute <avgPktSize, avgInterval> of a flow including ack flow.",
      //               MakeTraceSourceAccessor (&Ipv4DeflowRouting::m_qlen),
      //               "ns3::TracedCallback")
  ;

  return tid;
}



void Ipv4DeflowRouting::AddRouteEntry (const Ipv4Address dip, const uint32_t port) {
  NS_LOG_LOGIC (this << " Ipv4DeflowRouting::AddRouteEntry()");
  auto it = m_routeTable.find(dip);
  if (it == m_routeTable.end()) {
      m_routeTable[dip].assign(1, port);
  }else{
      m_routeTable[dip].push_back(port);
  }
}

std::vector<uint32_t> Ipv4DeflowRouting::GetAllCandidateEgressPorts (const Ipv4Address dip) const {
  auto it = m_routeTable.find(dip);
  std::vector<uint32_t> egressPorts;
  if (it == m_routeTable.end()) {
    std::cout << "Error in LookupRoutingTable(): found No matched routing entries" << std::endl;
  }else{
    egressPorts = it->second;
  }
  return egressPorts;
}

uint32_t Ipv4DeflowRouting::GetFlowIdByHashingFiveTuples(const Ptr<const Packet> packet, const Ipv4Header &header) const{
    uint32_t flowId = 0;
    Hasher m_hasher;
    m_hasher.clear();
    TcpHeader tcpHeader;
    packet->PeekHeader(tcpHeader);
    std::ostringstream oss;
    oss << header.GetSource() << " ";
    oss << header.GetDestination() << " ";
    oss << header.GetProtocol() << " ";
    oss << tcpHeader.GetSourcePort() << " ";
    oss << tcpHeader.GetDestinationPort();
    std::string data = oss.str();
    flowId = m_hasher.GetHash32(data);
    return flowId;
}


std::string Ipv4DeflowRouting::GetFlowStrOfFiveTuples(const Ptr<const Packet> packet, const Ipv4Header &header) const{
    Hasher m_hasher;
    m_hasher.clear();
    TcpHeader tcpHeader;
    packet->PeekHeader(tcpHeader);
    std::ostringstream oss;
    oss << header.GetSource() << " ";
    oss << header.GetDestination() << " ";
    oss << header.GetProtocol() << " ";
    oss << tcpHeader.GetSourcePort() << " ";
    oss << tcpHeader.GetDestinationPort();
    std::string data = oss.str();
    return data;
}

FlowletEntry Ipv4DeflowRouting::GetFlowletEntry(const std::string flowStr) const {
  auto it = m_flowletTable.find(flowStr);
  if (it == m_flowletTable.end()) {
    return FlowletEntry();
  }else{
    return it->second;
  }
}

void Ipv4DeflowRouting::SetFlowletEntry(const std::string flowStr, const uint32_t port, const uint64_t pktSize){
  m_flowletTable[flowStr] = FlowletEntry(port, pktSize);
  if (m_loadbalancebAlgorithm == LoadBalancingAlgorithm::DEFLOW)  {
    m_flowletTable[flowStr].avgPktGapInNs = m_packetGapThreshInNs * m_packetGapExpandFactor;
    m_flowletTable[flowStr].avgPktSizeInByte = m_packetSizeThreshInByte*m_packetSizeShrinkFactor*m_packetSizeAgingFactor + (1-m_packetSizeAgingFactor)*pktSize;
  }
  return ;
}



void Ipv4DeflowRouting::UpdateFlowletEntry(const std::string flowStr,const uint64_t pktSize){
  auto it = m_flowletTable.find(flowStr);
  it->second.hitTimeInNs = Simulator::Now().GetNanoSeconds();
  it->second.pktCnt += 1;
  it->second.byteCnt += pktSize;
  if (m_loadbalancebAlgorithm == LoadBalancingAlgorithm::DEFLOW)  {
    uint64_t curPktGap = Now().GetNanoSeconds() - it->second.hitTimeInNs;
    it->second.avgPktGapInNs = it->second.avgPktGapInNs*m_packetGapAgingFactor + curPktGap * (1-m_packetGapAgingFactor);
    it->second.avgPktSizeInByte = it->second.avgPktSizeInByte*m_packetSizeAgingFactor + pktSize*(1-m_packetSizeAgingFactor);
  }
  return ;
}

void Ipv4DeflowRouting::UpdateFlowletEntry(const std::string flowStr, const uint32_t port, const uint64_t pktSize){
  FlowletEntry * flet = &(m_flowletTable[flowStr]);
  flet->egressPort = port;
  if (m_loadbalancebAlgorithm == LoadBalancingAlgorithm::DEFLOW)  {
    uint64_t curPktGap = Simulator::Now().GetNanoSeconds() - flet->hitTimeInNs;
    flet->avgPktGapInNs = flet->avgPktGapInNs*m_packetGapAgingFactor + curPktGap * (1-m_packetGapAgingFactor);
    flet->avgPktSizeInByte = flet->avgPktSizeInByte*m_packetSizeAgingFactor + pktSize*(1-m_packetSizeAgingFactor);
  }
  flet->startTimeInNs = Simulator::Now().GetNanoSeconds();
  flet->hitTimeInNs = Simulator::Now().GetNanoSeconds();
  flet->pktCnt = 1;
  flet->byteCnt = pktSize;
  return ;
}


DrillEntry Ipv4DeflowRouting::GetDrillEntry(const Ipv4Address dip) const {
  auto it = m_drillTable.find(dip);
  if (it == m_drillTable.end()) {
    return DrillEntry();
  }else{
    return it->second;
  }
}

void Ipv4DeflowRouting::SetDrillEntry(const Ipv4Address dip, const uint64_t pktSize, const uint32_t port, const uint32_t qlen) {
  m_drillTable[dip] = DrillEntry(pktSize, port, qlen);
  return ;
}

void Ipv4DeflowRouting::UpdateDrillEntry(const Ipv4Address dip, const uint64_t pktSize, const uint32_t port, const uint32_t qlen) {
  auto it = m_drillTable.find(dip);
  if (it == m_drillTable.end()) {
    std::cout << "Error in UpdateDrillEntry(): no matched entry" << std::endl;
  }else{
    if (port == it->second.egressPort) {
      it->second.pktCnt += 1;
      it->second.byteCnt += pktSize;
    }else{
      it->second.pktCnt = 1;
      it->second.byteCnt = pktSize;
      it->second.startTimeInNs = Simulator::Now().GetNanoSeconds();
      it->second.egressPort = port;

    }
    it->second.qlenInByte = qlen;
  }
  return ;
}

void Ipv4DeflowRouting::UpdateDrillEntry(const Ipv4Address dip, const uint64_t pktSize, const uint32_t qlen) {
  auto it = m_drillTable.find(dip);
  if (it == m_drillTable.end()) {
    std::cout << "Error in UpdateDrillEntry(): no matched entry" << std::endl;
  }else{
    it->second.pktCnt += 1;
    it->second.byteCnt += pktSize;
    it->second.qlenInByte = qlen;
  }
  return ;
}


void Ipv4DeflowRouting::UpdateDrillEntry(const Ipv4Address dip, const uint64_t pktSize) {
  auto it = m_drillTable.find(dip);
  if (it == m_drillTable.end()) {
    std::cout << "Error in UpdateDrillEntry(): no matched entry" << std::endl;
  }else{
    it->second.pktCnt += 1;
    it->second.byteCnt += pktSize;
  }
  return ;
}


bool Ipv4DeflowRouting::GetFlowClassification(const FlowletEntry & flet, const uint64_t pktSize){
  if (flet.egressPort == 0) {
    return FlowClassification::NONLARGEFLOW;
  }else{
    NS_LOG_INFO("Identify the flowlet of large flows for flowlet: " << flet.ToString() << " with packet size: " << pktSize);
    NS_LOG_INFO("GapThresholdInNs: " << m_packetGapThreshInNs << ", SizeThresholdInByte: " << m_packetSizeThreshInByte);
    uint64_t curPktGap = Now().GetNanoSeconds() - flet.hitTimeInNs;
    double newPktGap = flet.avgPktGapInNs*m_packetGapAgingFactor + curPktGap * (1-m_packetGapAgingFactor);
    NS_LOG_INFO("AvgGap: " << flet.avgPktGapInNs << "ns, curGap: " << curPktGap << "ns, newGap: " << newPktGap <<"ns");
    double newPktSize = flet.avgPktSizeInByte*m_packetSizeAgingFactor + pktSize*(1-m_packetSizeAgingFactor);
    NS_LOG_INFO("AvgSize: " << flet.avgPktSizeInByte << "ns, curSize: " << pktSize << "ns, newSize: " << newPktSize <<"ns");

    if ((newPktGap <= m_packetGapThreshInNs)&&(newPktSize >= m_packetSizeThreshInByte)) {
      NS_LOG_INFO("LargeFlow!");
      return FlowClassification::LARGEFLOW;
    }else{
      NS_LOG_INFO("Non-LargeFlow!");
      return FlowClassification::NONLARGEFLOW;
    }
  }
}


bool Ipv4DeflowRouting::GetFlowClassificationByPort(uint16_t port){
  if (Ipv4DeflowRouting::MapOfport2flowSize.find(port) != Ipv4DeflowRouting::MapOfport2flowSize.end())  {
    return true;
  }
  return false;
}




uint32_t Ipv4DeflowRouting::GetBetterEgressPort(const std::vector<uint32_t> & ports, const uint32_t k) const {
  NS_LOG_INFO("Pick the best egress port of the "<< k << " randomly selected ports out of the total " << ports.size() << " ports");
  uint32_t portNum = ports.size();
  uint32_t betterPort = 0;
  if (portNum != 0){
        uint32_t minQlen = std::numeric_limits<uint32_t>::max ();
        for (uint32_t i = 0; i < k; i++) {
                uint32_t rndPort =  ports[std::rand() % portNum];
                uint32_t rndQlen = GetQueueLength(rndPort);
                NS_LOG_INFO("The " << i+1 << " randomly selected port: " << rndPort << ", Qlen: " << rndQlen);
                if (rndQlen <= minQlen) {
                  minQlen = rndQlen;
                  betterPort = rndPort;
                }
        }
    NS_LOG_INFO("The Better Port is: " << betterPort << ", Qlen: " << minQlen);
  }else{
    NS_LOG_INFO("Error! No canidate egress Port");
  }
  return betterPort;
}


uint32_t Ipv4DeflowRouting::GetTheBestEgressPort (const std::vector<uint32_t> &allEgressPorts, const Ptr<const Packet> packet, const Ipv4Header &header) {
  uint32_t egressPortsNum = allEgressPorts.size();
  if (egressPortsNum == 0) {
    std::cout << "Error in GetTheBestEgressPort(): no candidate egress ports" << std::endl;
    return 0;
  }
  switch (m_loadbalancebAlgorithm) {
    case LoadBalancingAlgorithm::ECMP:{
      NS_LOG_INFO("Apply LoadBalancingAlgorithm: " << "ECMP");
      uint32_t flowId = GetFlowIdByHashingFiveTuples(packet, header);
      // Get
      uint32_t egressPort =  allEgressPorts[flowId % egressPortsNum];
      return egressPort;
    }
    case LoadBalancingAlgorithm::RPS:{
       NS_LOG_INFO("Apply LoadBalancingAlgorithm: " << "RPS");
      uint32_t egressPort =  allEgressPorts[std::rand() % egressPortsNum];
      return egressPort;
    }
    case LoadBalancingAlgorithm::DRILL:{
      NS_LOG_INFO("Apply LoadBalancingAlgorithm: " << "DRILL");
      uint32_t betterEgressPort = GetBetterEgressPort(allEgressPorts, 1);
      uint32_t minQlen = GetQueueLength(betterEgressPort);
      Ipv4Address dip = header.GetDestination();
      DrillEntry drillEntry =  GetDrillEntry(dip);
      if (drillEntry.egressPort != 0) {
        NS_LOG_INFO("Exist Drill Entry");
        NS_LOG_INFO("The Exist Drill Entry is " << drillEntry.ToString());
        // if (drillEntry.egressPort == betterEgressPort) {
        //   NS_LOG_INFO("The exist Drill Entry is the same as the currently selected port, just update the Drill Entry");
        //   UpdateDrillEntry(dip, packet->GetSize(), minQlen);
        //   NS_LOG_INFO("The Update Drill Entry is " << GetDrillEntry(dip).ToString());
        //   return betterEgressPort;
        // }else
        
         if (drillEntry.qlenInByte < minQlen) {
          NS_LOG_INFO("The exist Drill Entry is better,  directly pick the previously selected port: " << drillEntry.egressPort);
          UpdateDrillEntry(dip, packet->GetSize());
          NS_LOG_INFO("The Update Drill Entry is " << GetDrillEntry(dip).ToString());
          return drillEntry.egressPort;
        }else{
          NS_LOG_INFO("The newly randomly selected port is better,  Just pick the new one: " << betterEgressPort);
          UpdateDrillEntry(dip, packet->GetSize(), betterEgressPort, minQlen);
          NS_LOG_INFO("The Update Drill Entry is " << GetDrillEntry(dip).ToString());
          return betterEgressPort;
        }
      }else{
        NS_LOG_INFO("New Drill Entry, directly pick the randomly selected better port: " << betterEgressPort);
        SetDrillEntry(dip, packet->GetSize(), betterEgressPort, minQlen);
        NS_LOG_INFO("The newly Drill Entry is " << GetDrillEntry(dip).ToString());
        return betterEgressPort;
      }
    }
    case LoadBalancingAlgorithm::LETFLOW:{
      NS_LOG_INFO("Apply LoadBalancingAlgorithm: " << "LETFLOW");
      std::string flowStr = GetFlowStrOfFiveTuples(packet, header);
      FlowletEntry flet = GetFlowletEntry(flowStr);
      if (flet.egressPort == 0) {
        NS_LOG_INFO("New Flowlet");
        uint32_t egressPort =  allEgressPorts[std::rand() % egressPortsNum];
        SetFlowletEntry(flowStr, egressPort, packet->GetSize());
        NS_LOG_INFO("Directly pick the randomly selected better port: " << egressPort);
        NS_LOG_INFO("The newly Flowlet is " << GetFlowletEntry(flowStr).ToString());
        return egressPort;
      }else{
        NS_LOG_INFO("Exist Flowlet: " << GetFlowletEntry(flowStr).ToString());
        uint64_t now = Simulator::Now().GetMicroSeconds();
        uint64_t gap = now - flet.hitTimeInNs/1000;
        NS_LOG_INFO("Gap: " << gap << "us, Timeout: " << m_flowletTimoutInUs << "us");
        if (gap >= m_flowletTimoutInUs) {
          uint32_t egressPort =  allEgressPorts[std::rand() % egressPortsNum];
          NS_LOG_INFO("Expired! randomly select a new port: " << egressPort);
          UpdateFlowletEntry(flowStr, egressPort, packet->GetSize());
          NS_LOG_INFO("The updated Flowlet is " << GetFlowletEntry(flowStr).ToString());
          return egressPort;
        }else{
          NS_LOG_INFO("Valid! use the previous selected port: " << flet.egressPort);
          UpdateFlowletEntry(flowStr, packet->GetSize());
          NS_LOG_INFO("The updated Flowlet is " << GetFlowletEntry(flowStr).ToString());
          return flet.egressPort;
        }
      }
    }
    // case LoadBalancingAlgorithm::DEFLOW:{
    //   NS_LOG_INFO("Apply LoadBalancingAlgorithm: " << "DEFLOW");
    //   std::string flowStr = GetFlowStrOfFiveTuples(packet, header);
    //   FlowletEntry flet = GetFlowletEntry(flowStr);
    //   if (flet.egressPort == 0) {
    //     NS_LOG_INFO("New DeFlow Entry");
    //     uint32_t egressPort =  GetBetterEgressPort(allEgressPorts, 2);
    //     SetFlowletEntry(flowStr, egressPort, packet->GetSize());
    //     NS_LOG_INFO("Directly pick the randomly selected better port: " << egressPort);
    //     NS_LOG_INFO("The newly DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
    //     return egressPort;
    //   }else{
    //       NS_LOG_INFO("Exist DeFlow Entry: " << GetFlowletEntry(flowStr).ToString());
    //       if ((GetQueueLength(flet.egressPort) >= m_congestionThreshInByte) && (GetFlowClassification(flet, packet->GetSize()) == FlowClassification::LARGEFLOW)) {
    //           NS_LOG_INFO("Congested for Large Flow!");
    //           NS_LOG_INFO("The previously selected port is in congesttion: Qlen= " << GetQueueLength(flet.egressPort) << ", Threshold= " << m_congestionThreshInByte);
    //           NS_LOG_INFO("The flowlet is belonging to the large flow: Qlen= " << GetQueueLength(flet.egressPort) << ", Threshold= " << m_congestionThreshInByte);
    //           uint32_t betterEgressPort = GetBetterEgressPort(allEgressPorts, 2);
    //           UpdateFlowletEntry(flowStr, betterEgressPort, packet->GetSize());
    //           NS_LOG_INFO("The updated DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
    //           return betterEgressPort;
    //       }else if ((Simulator::Now().GetNanoSeconds() - flet.hitTimeInNs) >= m_flowletTimoutInUs*1000) {
    //           NS_LOG_INFO("Deflow Entry Expired!");
    //           NS_LOG_INFO("Gap: " << (Simulator::Now().GetNanoSeconds() - flet.hitTimeInNs) << "us, Timeout: " << m_flowletTimoutInUs*1000 << "ns");
    //           uint32_t betterEgressPort = GetBetterEgressPort(allEgressPorts, 2);
    //           NS_LOG_INFO("Expired! randomly select a new port: " << betterEgressPort);
    //           UpdateFlowletEntry(flowStr, betterEgressPort, packet->GetSize());
    //           NS_LOG_INFO("The updated DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
    //           return betterEgressPort;
    //       }else{
    //           NS_LOG_INFO("Deflow Entry Valid!");
    //           UpdateFlowletEntry(flowStr, packet->GetSize());
    //           NS_LOG_INFO("The updated DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
    //           return flet.egressPort;
    //       }
    //   }
    // }
    case LoadBalancingAlgorithm::DEFLOW:{
      NS_LOG_INFO("Apply LoadBalancingAlgorithm: " << "DEFLOW");
      std::string flowStr = GetFlowStrOfFiveTuples(packet, header);
      FlowletEntry flet = GetFlowletEntry(flowStr);
      if (flet.egressPort == 0) {
        NS_LOG_INFO("New DeFlow Entry");
        uint32_t egressPort =  GetBetterEgressPort(allEgressPorts, 2);
        SetFlowletEntry(flowStr, egressPort, packet->GetSize());
        NS_LOG_INFO("Directly pick the randomly selected better port: " << egressPort);
        NS_LOG_INFO("The newly DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
        return egressPort;
      }else{
          TcpHeader tcpHeader;
          packet->PeekHeader(tcpHeader);
          uint16_t appPort =  tcpHeader.GetSourcePort();

          NS_LOG_INFO("Exist DeFlow Entry: " << GetFlowletEntry(flowStr).ToString());
          if ((GetQueueLength(flet.egressPort) >= m_congestionThreshInByte) && (GetFlowClassificationByPort(appPort))) {
              NS_LOG_INFO("Congested for Large Flow!");
              NS_LOG_INFO("The previously selected port is in congesttion: Qlen= " << GetQueueLength(flet.egressPort) << ", Threshold= " << m_congestionThreshInByte);
              NS_LOG_INFO("The flowlet is belonging to the large flow: Qlen= " << GetQueueLength(flet.egressPort) << ", Threshold= " << m_congestionThreshInByte);
              uint32_t betterEgressPort = GetBetterEgressPort(allEgressPorts, 1);
              UpdateFlowletEntry(flowStr, betterEgressPort, packet->GetSize());
              NS_LOG_INFO("The updated DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
              return betterEgressPort;
          }else if(GetQueueLength(flet.egressPort) >= m_heavyCongestionThreshInByte){
              NS_LOG_INFO("Heavy congestion!");
              uint32_t betterEgressPort = GetBetterEgressPort(allEgressPorts, 1);
              NS_LOG_INFO("Expired! randomly select a new port: " << betterEgressPort);
              UpdateFlowletEntry(flowStr, betterEgressPort, packet->GetSize());
              NS_LOG_INFO("The updated DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
              return betterEgressPort;
          }
          else if ((Simulator::Now().GetNanoSeconds() - flet.hitTimeInNs) >= m_flowletTimoutInUs*1000) {
              NS_LOG_INFO("Deflow Entry Expired!");
              NS_LOG_INFO("Gap: " << (Simulator::Now().GetNanoSeconds() - flet.hitTimeInNs) << "us, Timeout: " << m_flowletTimoutInUs*1000 << "ns");
              uint32_t betterEgressPort = GetBetterEgressPort(allEgressPorts, 1);
              NS_LOG_INFO("Expired! randomly select a new port: " << betterEgressPort);
              UpdateFlowletEntry(flowStr, betterEgressPort, packet->GetSize());
              NS_LOG_INFO("The updated DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
              return betterEgressPort;
          }else{
              NS_LOG_INFO("Deflow Entry Valid!");
              UpdateFlowletEntry(flowStr, packet->GetSize());
              NS_LOG_INFO("The updated DeFlow Entry is " << GetFlowletEntry(flowStr).ToString());
              return flet.egressPort;
          }
      }
    }
    
    default:{
      NS_FATAL_ERROR ("Unknown LoadBalancingAlgorithm type");
      return 0;
    }
    }
}

Ptr<Ipv4Route> Ipv4DeflowRouting::ConstructIpv4Route (uint32_t port, Ipv4Address destAddress) {
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (port); // get the netdevice linked to the port-th port/netdevice
  Ptr<Channel> channel = dev->GetChannel (); // get the channel linked to the port-th port
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





Ptr<Ipv4Route> Ipv4DeflowRouting::RouteOutput (Ptr<Packet> packet, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) {
    auto tmpNode = m_ipv4->GetObject<Node> ();
    Ipv4Address destAddress = header.GetDestination();
    NS_LOG_INFO ("########## RouteOutput(), Node: "<< GetNodeName(tmpNode, true) << ", Time: "<< Now().GetNanoSeconds() << "ns##################");

    if ((!packet) && (tmpNode->GetNDevices()==2)) {
      NS_LOG_DEBUG ("src host routing, Packet is == 0");
      Ptr<Ipv4Route> route = ConstructIpv4Route(1, destAddress);
      return route; // later
    }
    if(!packet){
      std::cout << "Error and do not know how to process" << std::endl;
      return 0;
    }

    std::vector<uint32_t> candidateEgressPorts = GetAllCandidateEgressPorts (destAddress);
    if (candidateEgressPorts.size() != 1) {
      std::cout << "Error in Ipv4DeflowRouting::RouteOutput() with " << candidateEgressPorts.size()  << " egress ports." << std::endl;
      return 0;
    }
    uint32_t bestEgressPort = GetTheBestEgressPort (candidateEgressPorts, packet, header);
    if (bestEgressPort != 1) {
      std::cout << "Error in Ipv4DeflowRouting::RouteOutput() with wrong egress port " << bestEgressPort << std::endl;
      return 0;
    }
    Ptr<Ipv4Route> route = ConstructIpv4Route(bestEgressPort, destAddress);
  return route;
}



bool Ipv4DeflowRouting::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                              UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)  {
    auto tmpNode = m_ipv4->GetObject<Node> ();
    NS_LOG_INFO ("########## RouteInput(), Node: "<< GetNodeName(tmpNode, true) << ", Time: "<< Now().GetNanoSeconds() << "ns, Size:" << p->GetSize() <<" ##################");
    // NS_LOG_LOGIC(this << " RouteInput: " << p << "Ip header: " << header);
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);

    uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);
    if (m_ipv4->IsDestinationAddress (header.GetDestination (), iif)) {
        if (!lcb.IsNull ()) {
            NS_LOG_LOGIC ("Local delivery to " << header.GetDestination ());
            TcpHeader tcpHeader;
            p->PeekHeader(tcpHeader);
            NS_LOG_INFO ("DIP: "<< header.GetDestination() << ", Seq: " << tcpHeader.GetSequenceNumber ().GetValue());
            lcb (p, header, iif);
            return true;
          }
        else {
            // The local delivery callback is null.  This may be a multicast or broadcast packet, so return false so that another multicast routing protocol can handle it.
            //  It should be possible to extend this to explicitly check whether it is a unicast packet, and invoke the error callback if so
            return false;
        }
      }



    Ptr<Packet> packet = ConstCast<Packet>(p);
    Ipv4Address destAddress = header.GetDestination();
    // only supports unicast
    if (destAddress.IsMulticast() || destAddress.IsBroadcast()) {
      return false;
    }
    // Check if input device supports IP forwarding
    if (m_ipv4->IsForwarding(iif) == false) {
      return false;
    }

    NS_LOG_INFO ("Destination IPv4Address: " << destAddress);
    std::vector<uint32_t> candidateEgressPorts = GetAllCandidateEgressPorts (destAddress);
    NS_LOG_INFO ("Candidate EgressPorts: " << vector2string(candidateEgressPorts));
    uint32_t bestEgressPort = GetTheBestEgressPort (candidateEgressPorts, packet, header);
    NS_LOG_INFO ("Selected EgressPorts: " << bestEgressPort);
    Ptr<Ipv4Route> route = ConstructIpv4Route(bestEgressPort, destAddress);
    ucb(route, packet, header);
    return true;
  }

void Ipv4DeflowRouting::NotifyInterfaceUp (uint32_t interface) {
}

void Ipv4DeflowRouting::NotifyInterfaceDown (uint32_t interface)  {
}

void Ipv4DeflowRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address) {
}

void Ipv4DeflowRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address){
}

void Ipv4DeflowRouting::SetIpv4 (Ptr<Ipv4> ipv4) {
  NS_LOG_LOGIC (this << "Setting up Ipv4: " << ipv4);
  NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
  m_ipv4 = ipv4;
}

void Ipv4DeflowRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const {
  
   auto tmpNode = m_ipv4->GetObject<Node> ();
   auto tmpNodeName = GetNodeName(tmpNode, true);
   std::ostream* os = stream->GetStream ();

   *os << "###################";
   *os << "Node: " << tmpNodeName
       << ", Time: " << Now().As (unit)
       << ", Local time: " << tmpNode->GetLocalTime ().As (unit)
       << ", Ipv4DeflowRouting table";
   *os << " ###################";
   *os << "\n";

    *os << std::setiosflags (std::ios::left) << std::setw (10) << "Index";
    *os << std::setiosflags (std::ios::left) << std::setw (20) << "DestionationIP";
    *os << std::setiosflags (std::ios::left) << std::setw (40) << "EgressPorts";
    *os << "\n";

    uint32_t index = 0;
    for (const auto& entry : m_routeTable) {
        *os << std::setiosflags (std::ios::left) << std::setw (10) << index++;
        *os << std::setiosflags (std::ios::left) << std::setw (20) << Ipv4Address2String(entry.first);
        *os << std::setiosflags (std::ios::left) << std::setw (40) << vector2string(entry.second, ", ");
        *os << std::endl;
    }
    return ;
}

void Ipv4DeflowRouting::DoDispose (void) {
  m_ipv4=0;
  Ipv4RoutingProtocol::DoDispose ();
}


}

