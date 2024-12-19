/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef DEFLOW_H
#define DEFLOW_H

/* add by ying on March 29, 2023 */
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-route.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/data-rate.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include<sstream>
#include <iostream>
#include <iomanip>
#include <string>



#include "ns3/hash.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/queue.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/core-module.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/net-device.h"
#include "ns3/channel.h"
#include "ns3/node.h"
#include "ns3/flow-id-tag.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/callback.h"
#include <algorithm>
#include "ns3/make-event.h"
// #include "ns3/userdefinedfunction.h"

 
#define UNSET_EGRESS_PORT_INDEX 0
#define UNSET_FLOWLET_END_TIME_IN_NS 0


namespace ns3 {

std::string GetNodeName(const Ptr<Node> node, bool withIndex=false);
std::string Ipv4Address2String(Ipv4Address addr);

template <typename T>
std::string vector2string (std::vector<T> src, std::string c=", ") {
  std::string str = "[";
  uint32_t vecSize = src.size();
  if (vecSize == 0) {
    str = str + "NULL]";
    return str;
  }else{
    for (uint32_t i = 0; i < vecSize-1; i++) {
      std::string curStr = std::to_string(src[i]);
      str = str + curStr + c;
    }
    std::string curStr = std::to_string(src[vecSize-1]);
    str = str + curStr + "]";
    return str;
  }
}



enum LoadBalancingAlgorithm  {
  ECMP = 0,
  RPS = 1,
  DRILL = 2,
  LETFLOW = 3,
  DEFLOW = 4,
  NONE = 5
};

enum FlowClassification  {
  NONLARGEFLOW=0,
  LARGEFLOW = 1
};


struct FlowletEntry {
  // std::string flowStr; 
  uint32_t egressPort;
  uint64_t startTimeInNs;
  uint64_t hitTimeInNs;
  uint64_t pktCnt;
  uint64_t byteCnt;
  double avgPktGapInNs; // alpha
  double avgPktSizeInByte; // beta
  void print(){
    std::cout << "FlowletEntry Info-----> ";
    // std::cout << "flowStr: " << flowStr << ", ";
    std::cout << "egressPort:" << egressPort << ", ";
    std::cout << "startTimeInNs:" << startTimeInNs << "ns, ";
    std::cout << "hitTimeInNs:" << hitTimeInNs << "ns, ";
    std::cout << "pktCnt:" << pktCnt << ", ";
    std::cout << "byteCnt:" << byteCnt << ", ";
    std::cout << "avgPktGapInNs:" << avgPktGapInNs << "ns, ";
    std::cout << "avgPktSizeInByte:" << avgPktSizeInByte << "byte, ";
    std::cout << std::endl;
  }

  FlowletEntry() {
    startTimeInNs = Simulator::Now().GetNanoSeconds();
    hitTimeInNs = Simulator::Now().GetNanoSeconds();
    pktCnt = 0;
    egressPort = 0;
  }

  FlowletEntry(const uint32_t port, const uint64_t pktSize) {
    startTimeInNs = Simulator::Now().GetNanoSeconds();
    hitTimeInNs = Simulator::Now().GetNanoSeconds();
    pktCnt = 1;
    byteCnt = pktSize;
    egressPort = port;
  }


  std::string ToString() const {
    std::ostringstream os;
    os << std::setiosflags (std::ios::left) << std::setw (10) << egressPort;
    os << std::setiosflags (std::ios::left) << std::setw (10) << startTimeInNs;
    os << std::setiosflags (std::ios::left) << std::setw (10) << hitTimeInNs;
    os << std::setiosflags (std::ios::left) << std::setw (10) << pktCnt;
    os << std::setiosflags (std::ios::left) << std::setw (10) << byteCnt;
    os << std::setiosflags (std::ios::left) << std::setw (7) << std::fixed << std::setprecision(1) << 1.0*avgPktGapInNs/1000;
    os << std::setiosflags (std::ios::left) << std::setw (7) << std::fixed << std::setprecision(1) << avgPktSizeInByte;
    return os.str();
  }


};

struct DrillEntry {
  uint32_t qlenInByte;
  uint32_t egressPort;
  uint64_t startTimeInNs;
  uint64_t pktCnt;
  uint64_t byteCnt;
  void print(){
    std::cout << "DrillEntry Info-----> ";
    // std::cout << "flowStr: " << flowStr << ", ";
    std::cout << "egressPort:" << egressPort << ", ";
    std::cout << "startTime:" << startTimeInNs << " ns, ";
    std::cout << "qlen:" << qlenInByte << " bytes, ";
    std::cout << "pktCnt:" << pktCnt << ", ";
    std::cout << "byteCnt:" << byteCnt << ", ";
    std::cout << std::endl;
  }

  DrillEntry() {
    startTimeInNs = Simulator::Now().GetNanoSeconds();
    egressPort = 0;
    qlenInByte = 0;
    pktCnt = 0;
    byteCnt = 0;
  }
  DrillEntry(uint64_t pktSize, uint32_t port, uint32_t qlen) {
    startTimeInNs = Now().GetNanoSeconds();
    qlenInByte = qlen;
    pktCnt = 1;
    egressPort = port;
    byteCnt = pktSize;
  }

  std::string ToString(){
    std::ostringstream os;
    os << std::setiosflags (std::ios::left) << std::setw (10) << egressPort;
    os << std::setiosflags (std::ios::left) << std::setw (10) << qlenInByte;
    os << std::setiosflags (std::ios::left) << std::setw (10) << startTimeInNs;
    os << std::setiosflags (std::ios::left) << std::setw (10) << pktCnt;
    os << std::setiosflags (std::ios::left) << std::setw (10) << byteCnt;
    return os.str();
  }

};




class Ipv4DeflowRouting : public Ipv4RoutingProtocol
{


public:
  Ipv4DeflowRouting ();
  ~Ipv4DeflowRouting ();

  static TypeId GetTypeId (void);
  static std::map<uint16_t, uint32_t> MapOfport2flowSize;

  void AddRouteEntry (const Ipv4Address dip, const uint32_t port);
  uint32_t GetTheBestEgressPort (const std::vector<uint32_t> &allEgressPorts, const Ptr<const Packet> packet, const Ipv4Header &header) ;
  uint32_t GetFlowIdByHashingFiveTuples(const Ptr<const Packet> packet, const Ipv4Header &header) const;
  uint32_t GetQueueLength (const uint32_t interface) const;
  DrillEntry GetDrillEntry(const Ipv4Address dip) const;
  void SetDrillEntry(const Ipv4Address dip, const uint64_t pktSize, const uint32_t port, const uint32_t qlen);
  void UpdateDrillEntry(const Ipv4Address dip, const uint64_t pktSize, const uint32_t port, const uint32_t qlen) ;
  void UpdateDrillEntry(const Ipv4Address dip, const uint64_t pktSize);
  void UpdateDrillEntry(const Ipv4Address dip, const uint64_t pktSize, const uint32_t qlen) ;
  std::string GetFlowStrOfFiveTuples(const Ptr<const Packet> packet, const Ipv4Header &header) const;
  FlowletEntry GetFlowletEntry(const std::string flowStr) const;
  void SetFlowletEntry(const std::string flowStr, const uint32_t port, const uint64_t pktSize);
  void UpdateFlowletEntry(const std::string flowStr,const uint64_t pktSize);
  void UpdateFlowletEntry(const std::string flowStr, const uint32_t port, const uint64_t pktSize);
  uint32_t GetBetterEgressPort(const std::vector<uint32_t> & ports, const uint32_t k) const;

  bool GetFlowClassificationByPort(uint16_t port);


  /* Inherit From Ipv4RoutingProtocol */
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                           UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                           LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

  virtual void DoDispose (void);

  std::vector<uint32_t> LookupRoutingTable (Ipv4Address dest);
  Ptr<Ipv4Route> ConstructIpv4Route (uint32_t port, Ipv4Address destAddress);
  std::vector<uint32_t> GetAllCandidateEgressPorts (const Ipv4Address dip) const;
  bool GetFlowClassification(const FlowletEntry & flet, const uint64_t pktSize);
  void PrintQueueLen (std::string ctx, uint32_t oldValue, uint32_t newValue);
private:
  // Ipv4 associated with this router
  Ptr<Ipv4> m_ipv4;
  std::map<Ipv4Address, std::vector<uint32_t> > m_routeTable;
  std::map<Ipv4Address, DrillEntry> m_drillTable;
  std::map<std::string, FlowletEntry> m_flowletTable;
  uint64_t m_flowletTimoutInUs;

  double m_packetGapThreshInNs;
  double m_packetGapExpandFactor;
  double m_packetGapAgingFactor;

  double m_packetSizeThreshInByte;
  double m_packetSizeShrinkFactor;
  double m_packetSizeAgingFactor;

  double m_congestionThreshInByte;
  double m_heavyCongestionThreshInByte;

  LoadBalancingAlgorithm m_loadbalancebAlgorithm;
  TracedValue<uint32_t> m_qlen; //
	// TracedCallback<const Ptr<const Packet>, const Ipv4Header & header > m_flowClassification; // the trace for printing dequeue

};




}

#endif /* DEFLOW_H */

