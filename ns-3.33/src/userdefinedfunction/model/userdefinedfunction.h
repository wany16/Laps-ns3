/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef USERDEFINEDFUNCTION_HD
#define USERDEFINEDFUNCTION_HD

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdio>
#include <vector>
#include <map>
#include <utility>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
#include <functional>
#include <iostream>
#include <string>
#include <stdint.h>
#include <ctime>
#include <numeric>

#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/hash.h"
#include "ns3/tcp-header.h"
#include "ns3/mac48-address.h"
#include <ns3/rdma.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-driver.h>
#include <ns3/switch-node.h>
#include <ns3/sim-setting.h>
#include "ns3/bulk-send-application.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/boolean.h"
#include "ns3/common-user-model.h"
#include "ns3/rdma-smartflow-routing.h"

#define BYTE_NUMBER_PER_GBPS 1000000000 // 1Gbps
#define TG_CDF_TABLE_ENTRY 32
#define LINK_LATENCY_IN_NANOSECOND 10000          // 10us
#define THRESHOLD_IN_BYTE_FOR_LARGE_FLOW 10000000 // 10MB
#define THRESHOLD_IN_BYTE_FOR_SMALL_FLOW 100000   // 100KB

#define DEFAULT_PRIOTIY_OF_GLOBAL_ROUTING 0
#define DEFAULT_PRIOTIY_OF_SMARTFLOW_ROUTING 20
#define DEFAULT_NUMBER_OF_APP_START_PORT 1024
#define DEFAULT_DISPLAY_INTERVAL_IN_NANOSECOND 10000000 // 10ms
#define DEFAULT_UDP_ECHO_PORT_NUMBER 6666
#define DEFAULT_UDP_ECHO_START_TIME_IN_SECOND 0
#define DEFAULT_UDP_ECHO_END_TIME_IN_SECOND 1
#define DEFAULT_UDP_ECHO_PKT_INTERVAL_IN_NANOSECOND 1000 // 1us
#define DEFAULT_UDP_ECHO_PKT_SIZE_IN_BYTE 777            // 666 bytes
// #define DEFAULT_MAX_TCP_MSS_IN_BYTE 1400 // tcp optiaons 开启
#define DEFAULT_PRECISION_FOR_FLOAT_TO_STRING 4

#define SWITCH_NODE_TYPE 1
#define SERVER_NODE_TYPE 0

#define FLAG_ENABLE 1
#define FLAG_DISABLE 0
#define ECN_PARA_NUMBER 4
#define KV_CACHE_INCAST 1
#define KV_CACHE_BROADCAST 2
#define KV_CACHE_INCA 3
#define KV_CACHE_RING 4

namespace ns3
{

  template <typename T1, typename T2>
  std::string PairVector2string(std::vector<std::pair<T1, T2>> &src, std::string c = ", ")
  {
    std::string str = "[";
    uint32_t vecSize = src.size();
    if (vecSize == 0)
    {
      str = str + "NULL]";
      return str;
    }
    else
    {
      for (uint32_t i = 0; i < vecSize - 1; i++)
      {
        std::string curStr = std::to_string(src[i].first) + ":" + std::to_string(src[i].second);
        str = str + curStr + c;
      }
      std::string curStr = std::to_string(src[vecSize - 1].first) + ":" + std::to_string(src[vecSize - 1].second);
      str = str + curStr + "]";
      return str;
    }
  }

  struct ecn_para_entry_t
  {
    uint32_t kminInKb;
    uint32_t kmaxInKb;
    double pmax;

    void print()
    {
      std::cout << "ECN Para: ";
      std::cout << "kminInKb=" << kminInKb << "Kb, ";
      std::cout << "kmaxInKb=" << kmaxInKb << "Kb, ";
      std::cout << "pmax=" << pmax << " ";
      std::cout << std::endl;
    }
    std::string toStrinng(){
      std::string res = "[";
      res += std::to_string(kminInKb) + ", ";
      res += std::to_string(kmaxInKb) + ", ";
      res += std::to_string(pmax) + "]";
      return res;
    }
  };

  struct nic_para_entry_t
  {

    bool enableClampTargetRate;     // 1 : reduce each time cnp arrives, decrease faster when receiving a CNP, the target rate is always updated to be the current rate
    double alphaResumeIntervalInUs; // 链路层拥塞后恢复到正常的微秒数, 默认值55
    double rpTimerInUs;             // 发送端速率计时器周期, 默认值1500微秒
    uint32_t fastRecoveryTimes;     // 速率快速恢复次数, 默认值5
    std::string ewmaGain;                // 速率降低系数/级别, 默认值1/16
    double rateAiInMbps;            // Rate increment unit in AI period, 默认值5Mbps
    double rateHaiInMbps;           // Rate increment unit in HAI period, 默认值50Mbps
    bool enableL2BackToZero;        // 检测到网络拥塞后将链路层的信用值重置为零, 默认值0代表关闭
    uint32_t l2ChunkSizeInByte;     // 0 for disable
    uint32_t l2AckIntervalInByte;   // 每隔多少字节生成ack，没太弄明白。
    uint32_t ccMode;
    double rateDecreaseIntervalInUs; // The interval of rate decrease check, 默认值4
    uint32_t minRateInMbps;          // Minimum rate of a throttled flow in Mbps, 默认值100
    uint32_t mtuInByte;
    uint32_t miThresh;         // Threshold of number of consecutive AI before MI, 默认5,脚本0
    bool enableVarWin;         // Use variable window size or not, 默认false
    bool enableFastFeedback;   // Fast React to congestion feedback, 默认true,脚本false
    bool enableMultiRate;      // Maintain multiple rates in HPCC, by default true, but shell 0/false
    bool enableSampleFeedback; // Whether sample feedback or not in HPCC, by default false
    double targetUtilization;  // The Target Utilization of the bottleneck bandwidth, by default 95%
    bool enableRateBound;      // Bound packet sending by rate, for test only, by default true
    double dctcpRateAiInMbps;
    double pintProbTresh;

    nic_para_entry_t(){
      enableClampTargetRate = false;     // 1 : reduce each time cnp arrives, decrease faster
      alphaResumeIntervalInUs = 0; // 链路层拥塞后恢复到正常的微秒数, 默认值55
      rpTimerInUs = 0;             // 发送端速率计时器周期, 默认值1500微秒
      fastRecoveryTimes = 0;     // 速率快速恢复次数, 默认值5
      ewmaGain = "";                // 速率降低系数/级别, 默认值1/16
      rateAiInMbps = 0;            // Rate increment unit in AI period, 默认值5Mbps
      rateHaiInMbps = 0;           // Rate increment unit in HAI period, 默认值50Mbps
      enableL2BackToZero = false;        // 检测到网络拥塞后将链路层的信用值重置为零, 默认值0代表关闭
      l2ChunkSizeInByte = 0;     // 0 for disable
      l2AckIntervalInByte = 0;   // 每隔多少字节生成ack，没太弄明白。
      ccMode = 0;
      rateDecreaseIntervalInUs = 0; // The interval of rate decrease check, 默认值4
      minRateInMbps = 0;          // Minimum rate of a throttled flow in Mbps, 默认值100
      mtuInByte = 0;
      miThresh = 0;         // Threshold of number of consecutive AI before MI, 默认5,脚本0
      enableVarWin = false;         // Use variable window size or not, 默认false
      enableFastFeedback = false;   // Fast React to congestion feedback, 默认true,脚本false
      enableMultiRate = false;      // Maintain multiple rates in HPCC, by default true, but shell 0/false
      enableSampleFeedback = false; // Whether sample feedback or not in HPCC, by default false
      targetUtilization = 0;  // The Target Utilization of the bottleneck bandwidth, by default 95%
      enableRateBound = false;      // Bound packet sending by rate, for test only, by default true
      dctcpRateAiInMbps = 0;
      pintProbTresh = 0;
    }



    void print()
    {
      std::cout << "RDMA NIC Para: ";
      std::cout << "enableClampTargetRate=" << enableClampTargetRate << ", ";
      std::cout << "alphaResumeIntervalInUs=" << alphaResumeIntervalInUs << ", ";
      std::cout << "rpTimerInUs=" << rpTimerInUs << ", ";
      std::cout << "fastRecoveryTimes=" << fastRecoveryTimes << ", ";
      std::cout << "ewmaGain=" << ewmaGain << ", ";
      std::cout << "rateAiInMbps=" << rateAiInMbps << ", ";
      std::cout << "rateHaiInMbps=" << rateHaiInMbps << ", ";
      std::cout << "enableL2BackToZero=" << enableL2BackToZero << ", ";
      std::cout << "l2ChunkSizeInByte=" << l2ChunkSizeInByte << ", ";
      std::cout << "l2AckIntervalInByte=" << l2AckIntervalInByte << ", ";
      std::cout << "ccMode=" << ccMode << ", ";
      std::cout << "rateDecreaseIntervalInUs=" << rateDecreaseIntervalInUs << ", ";
      std::cout << "minRateInMbps=" << minRateInMbps << ", ";
      std::cout << "mtuInByte=" << mtuInByte << ", ";
      std::cout << "miThresh=" << miThresh << ", ";
      std::cout << "enableVarWin=" << enableVarWin << ", ";
      std::cout << "enableFastFeedback=" << enableFastFeedback << ", ";
      std::cout << "enableMultiRate=" << enableMultiRate << ", ";
      std::cout << "enableSampleFeedback=" << enableSampleFeedback << ", ";
      std::cout << "targetUtilization=" << targetUtilization << ", ";
      std::cout << "enableRateBound=" << enableRateBound << ", ";
      std::cout << "dctcpRateAiInMbps=" << dctcpRateAiInMbps << ", ";
      std::cout << "pintProbTresh=" << pintProbTresh << ", ";
      std::cout << std::endl;
    }
  };

  std::string ipv4Address_to_string(Ipv4Address addr);

  template <typename T>
  std::string to_string(T src)
  {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(DEFAULT_PRECISION_FOR_FLOAT_TO_STRING);
    ss << src;
    std::string str = ss.str();
    return str;
  }

  template <typename T>
  std::string vector_to_string(std::vector<T> src)
  {
    std::string str = "[";
    uint32_t vecSize = src.size();
    if (vecSize == 0)
    {
      str = str + "NULL]";
      return str;
    }
    else
    {
      for (uint32_t i = 0; i < vecSize - 1; i++)
      {
        std::string curStr = to_string<T>(src[i]);
        str = str + curStr + ", ";
      }
      std::string curStr = to_string<T>(src[vecSize - 1]);
      str = str + curStr + "]";
      return str;
    }
  }

  template <typename K, typename V>
  std::string map_to_string(const std::map<K, V> &m)
  {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto &pair : m)
    {
      if (!first)
      {
        oss << ", ";
      }
      oss << to_string<K>(pair.first);
      oss << ":";
      oss << to_string<V>(pair.second);
      first = false;
    }
    oss << "}";
    return oss.str();
  }

  template <typename K>
  void insert_element_to_counter_map(const std::map<K, uint32_t> &m, K &key)
  {
    auto it = m.find(key);
    if (it == m.end())
    {
      m[key] = 1;
    }
    else
    {
      it->second += 1;
    }
  }

  struct addr_entry_t
  {
    Ipv4Address network;
    Ipv4Mask mask;
    Ipv4Address base;
  };

  /*struct reorder_entry_t
  {
    bool flag;
    std::vector<uint32_t> seqs;
    void print()
    {
      std::cout << "Reorder INFO" << ": ";
      if (flag == true)
      {
        std::cout << "Flag = True" << ", ";
      }
      else
      {
        std::cout << "Flag = False" << ", ";
      }
      std::cout << "SeqNums=" << vector_to_string<uint32_t>(seqs);
      std::cout << std::endl;
    }
  };*/

  struct cdf_entry
  {
    double value;
    double cdf;
  };

  struct est_entry_t
  {
    std::string name;
    std::string value;
  };

  /* CDF distribution */
  struct cdf_table
  {
    struct cdf_entry *entries;
    int num_entry;  /* number of entries in CDF table */
    int max_entry;  /* maximum number of entries in CDF table */
    double min_cdf; /* minimum value of CDF (default 0) */
    double max_cdf; /* maximum value of CDF (default 1) */
  };

  struct CHL_entry_t
  {
    uint32_t chlIdx;
    uint32_t srcNodeIdx;
    uint32_t dstNodeIdx;
    uint32_t widthInGbps;
    uint32_t widthInMbps;
    uint32_t delayInUs;
    std::string queueType; // droptail
    std::string queueSize;
    bool enablePfcMonitor;
    uint32_t kminInKb;
    uint32_t kmaxInKb;
    double pmax;
    void print()
    {
      std::cout << "CHL_entry: chlIdx=" << chlIdx << ", ";
      std::cout << "srcNodeIdx=" << srcNodeIdx << ", ";
      std::cout << "dstNodeIdx=" << dstNodeIdx << ", ";
      std::cout << "widthInGbps=" << widthInGbps << "Gbps, ";
      std::cout << "delayInUs=" << delayInUs << "Us, ";
      std::cout << std::endl;
    }
  };

  /*struct LatencyData
  {
    std::pair<uint32_t, uint32_t> latencyInfo;
    Time tsGeneration;
  };

  struct PathData
  {
    uint32_t pid;
    uint32_t priority;
    std::vector<uint32_t> portSequence;
    std::vector<uint32_t> nodeIdSequence;
    uint32_t latency;
    uint32_t theoreticalSmallestLatencyInNs;
    uint32_t pathDre;
    Time tsGeneration;
    Time tsProbeLastSend;
    Time tsLatencyLastSend;

    void print()
    {
      std::cout << "PIT_KEY: Pid=" << pid << ", ";
      std::cout << "PIT_DATA: Priority=" << priority << ", ";
      std::cout << "pathDre=" << pathDre << "ns, ";
      std::cout << "Delay=" << latency << "ns, ";
      std::cout << "thLatency=" << theoreticalSmallestLatencyInNs << "ns, ";
      std::cout << "GenTs=" << tsGeneration.GetNanoSeconds() << "ns, ";
      std::cout << "PrbTs=" << tsProbeLastSend.GetNanoSeconds() << "ns, ";
      std::cout << "SntTs=" << tsLatencyLastSend.GetNanoSeconds() << "ns, ";
      std::cout << "NodeIDs=" << vector_to_string<uint32_t>(nodeIdSequence) << ", ";
      std::cout << "PortIDs=" << vector_to_string<uint32_t>(portSequence) << " ";
      std::cout << std::endl;
    }
  };

  struct flet_entry_t
  {
    PathData *lastSelectedPitEntry;
    uint32_t lastSelectedTimeInNs;

    void print()
    {
      lastSelectedPitEntry->print();
      std::cout << "lastSelectedTimeInNs=" << lastSelectedTimeInNs << std::endl;
    }
  };

  struct pdt_entry_t
  {
    PathData *pit;
    uint32_t pid;
    uint32_t latency;

    void print()
    {
      // std::cout << "pid=" << pid << ", latency=" << latency << std::endl;
      pit->print();
    }
  };

  struct PathSelTblKey
  {
    Ipv4Address selfToRIp;
    Ipv4Address dstToRIp;

    PathSelTblKey(Ipv4Address &self, Ipv4Address &dest) : selfToRIp(self), dstToRIp(dest) {}
    bool operator<(const PathSelTblKey &other) const
    {
      if (this->selfToRIp < other.selfToRIp)
      {
        return true;
      }

      if (this->selfToRIp == other.selfToRIp)
      {
        return (this->dstToRIp < other.dstToRIp);
      }
      return false;
    }

    bool operator==(const PathSelTblKey &other) const
    {
      return this->selfToRIp == other.selfToRIp && this->dstToRIp == other.dstToRIp;
    }

    void print()
    {
      std::cout << "PST_KEY: selfToRIp=" << ipv4Address_to_string(selfToRIp) << ", ";
      std::cout << "dstToRIp=" << ipv4Address_to_string(dstToRIp) << std::endl;
    }
  };

  struct probeInfoEntry
  {
    uint32_t pathId;
    uint32_t probeCnt;
    probeInfoEntry(uint32_t pathId, uint32_t probeCnt)
    {
      pathId = pathId;
      probeCnt = probeCnt;
    }
    probeInfoEntry()
    {
      pathId = 0;
      probeCnt = 0;
    }
    void print()
    {
      std::cout << "Path Id: " << pathId << ", ";
      std::cout << "probeCnt=" << probeCnt << std::endl;
    }
  };

  struct pstEntryData
  {
    uint32_t pathNum;
    uint32_t lastSelectedPathIdx;
    uint32_t lastPiggybackPathIdx;
    uint32_t lastSelectedTimeInNs;
    uint32_t highestPriorityPathIdx;
    // uint32_t smallestLatencyInNs;
    std::vector<uint32_t> paths;
    void print()
    {
      std::cout << "PST_DATA: pathNum=" << pathNum << ", ";
      std::cout << "lastSelectedIdx=" << lastSelectedPathIdx << ", ";
      std::cout << "lastPiggybackPathIdx=" << lastPiggybackPathIdx << ", ";
      std::cout << "highestPriorityIdx=" << highestPriorityPathIdx << ", ";
      std::cout << "lastSelectedTimeInNs=" << lastSelectedTimeInNs << ", ";
      std::cout << "Paths=" << vector_to_string(paths) << std::endl;
    }
  };

  struct rbn_entry_t
  {
    uint32_t lastSelectedPathIdx;
    PathData *selectedPitEntry;
    void print()
    {
      std::cout << "rbn_entry_t INFO is as follows:" << std::endl;
      if (selectedPitEntry == 0)
      {
        std::cout << "selectedPitEntry is not set" << std::endl;
      }
      else
      {
        selectedPitEntry->print();
      }
      std::cout << "lastSelectedPathIdx=" << lastSelectedPathIdx << std::endl;
    }
  };*/

  /* CDF distribution */
  struct tfc_entry_t
  {
    uint32_t srcNodeIdx;
    uint32_t dstNodeIdx;
    double loadfactor;
    uint32_t capacityInGbps;
    long flowCount;
    long bytesCount;
    uint32_t smallFlowCount;
    uint32_t largeFlowCount;
    void print()
    {
      std::cout << "TFC ENTRY: srcNodeIdx=" << srcNodeIdx << ", ";
      std::cout << "dstNodeIdx=" << dstNodeIdx << ", ";
      std::cout << "loadfactor=" << loadfactor << ", ";
      std::cout << "capacityInGbps=" << capacityInGbps << ", ";
      std::cout << "flowCount=" << flowCount << ", ";
      std::cout << "bytesCount=" << bytesCount << ", ";
      std::cout << "smallFlowCount=" << smallFlowCount << ", ";
      std::cout << "largeFlowCount=" << largeFlowCount << std::endl;
    }
    void patternprint()
    {
      std::cout << "pattern ENTRY: srcNodeIdx=" << srcNodeIdx << ", ";
      std::cout << "dstNodeIdx=" << dstNodeIdx << std::endl;
    }
  };

  struct TOPO_t
  {
    uint32_t swNum;
    uint32_t svNum;
    std::vector<uint32_t> swNodeIdx;
    std::vector<uint32_t> svNodeIdx;
    uint32_t allNum;
  };

  struct vmt_entry_t
  {
    Ipv4Address torAddr;
    void print()
    {
      std::cout << "VMT_DATA: torAddr=" << ipv4Address_to_string(torAddr) << std::endl;
    }
  };

  template <typename T>
  void find_target_routingProtocol_index(const NodeContainer &nodes, uint32_t &idx)
  {
    Ptr<Ipv4RoutingProtocol> ptr_rt = 0;
    Ptr<Ipv4ListRouting> ptr_lrt = 0;
    ptr_rt = nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol();
    ptr_lrt = DynamicCast<Ipv4ListRouting>(ptr_rt);
    if (ptr_lrt == 0)
    {
      std::cout << "Error in findTargetRoutingProtocolIndex-->do not install ListRoutingProtocol" << std::endl;
      return;
    }
    int16_t priority = 0;
    for (uint32_t i = 0; i < ptr_lrt->GetNRoutingProtocols(); i++)
    {
      Ptr<Ipv4RoutingProtocol> listProto = ptr_lrt->GetRoutingProtocol(i, priority);
      Ptr<T> flag = DynamicCast<T>(listProto);
      if (flag != 0)
      {
        idx = i;
        return;
      }
    }
  }

  template <typename T>
  Ptr<T> get_the_target_routing_protocol_of_the_given_node(Ptr<Node> node)
  {
    Ptr<Ipv4RoutingProtocol> ipv4RoutingProtocol = node->GetObject<Ipv4>()->GetRoutingProtocol();
    Ptr<Ipv4ListRouting> ipv4ListRouting = DynamicCast<Ipv4ListRouting>(ipv4RoutingProtocol); // 将类型Ipv4RoutingProtocol转换为Ipv4ListRouting
    if (typeid(T) == typeid(Ipv4ListRouting))
    {
      return DynamicCast<T>(ipv4ListRouting);
    }
    if (ipv4ListRouting == 0)
    {
      std::cout << "ERROR in get_the_target_routing_protocol_of_the_given_node(), MUST install ipv4ListRouting first" << std::endl;
      return 0;
    }
    else
    {
      int16_t routing_protocol_priority = 0;
      uint32_t routing_protocol_index = 0;
      NodeContainer curNodeContainer;
      curNodeContainer.Add(node);
      find_target_routingProtocol_index<T>(curNodeContainer, routing_protocol_index);
      ipv4RoutingProtocol = ipv4ListRouting->GetRoutingProtocol(routing_protocol_index, routing_protocol_priority);
      Ptr<T> targetRouting = DynamicCast<T>(ipv4RoutingProtocol);
      return targetRouting;
    }
  }

  template <typename T>
  uint32_t install_routing_protocol_to_nodes(NodeContainer &nodes, int16_t priority)
  {
    uint32_t installedNodeCnt = 0;
    if (typeid(T) == typeid(Ipv4GlobalRouting))
    {
      Ipv4ListRoutingHelper lrp;
      Ipv4GlobalRoutingHelper grp;
      lrp.Add(grp, priority);
      InternetStackHelper stack;
      stack.SetRoutingHelper(lrp);
      stack.Install(nodes);
      installedNodeCnt = nodes.GetN();
    }
    else
    {
      for (uint32_t i = 0; i < nodes.GetN(); i++)
      {
        Ptr<Ipv4RoutingProtocol> node_rp = nodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol();
        Ptr<Ipv4ListRouting> node_lrp = DynamicCast<Ipv4ListRouting>(node_rp);
        if (node_lrp == 0)
        {
          std::cout << "ERROR, MUST install ipv4ListRouting and then install other protocols" << std::endl;
          continue;
        }
        else
        {
          Ptr<T> rp = CreateObject<T>();
          node_lrp->AddRoutingProtocol(rp, priority);
          installedNodeCnt = installedNodeCnt + 1;
        }
      }
    }
    return installedNodeCnt;
  }

  template <typename T1, typename T2>
  void update_EST(std::map<uint32_t, est_entry_t> &est, T1 name, T2 value)
  {
    est_entry_t estEntry;
    estEntry.name = to_string(name);
    estEntry.value = to_string(value);
    est[est.size()] = estEntry;
    return;
  }

  struct flow_entry_t
  {
    uint32_t idx;
    uint32_t prioGroup;
    uint32_t pktCnt;
    uint64_t byteCnt;
    uint64_t winInByte;
    uint64_t rttInNs;
    double startTimeInSec;
    Ipv4Address srcAddr;
    Ipv4Address dstAddr;
    uint32_t srcPort;
    uint32_t dstPort;
    Ptr<Node> srcNode;
    Ptr<Node> dstNode;
    uint32_t srcSvIdx;
    uint32_t dstSvIdx;

    // flow_entry_t(uint32_t idx, uint32_t prioGroup, uint32_t pktCnt, uint64_t byteCnt,
    //             uint32_t win, uint64_t baseRtt, uint64_t startTimeInNs,
    //             Ipv4Address srcAddr, Ipv4Address dstAddr, uint32_t srcPort, uint32_t dstPort) {
    //   idx = idx;
    //   prioGroup = prioGroup;
    //   pktCnt = pktCnt;
    //   byteCnt = byteCnt;
    //   win = win;
    //   baseRtt = baseRtt;
    //   srcAddr = srcAddr;
    //   dstAddr = dstAddr;
    //   srcPort = srcPort;
    //   dstPort = dstPort;
    //   startTimeInNs = startTimeInNs;
    // }
    void print()
    {
      std::cout << "Flow Entry: ";
      std::cout << "idx=" << idx << ", ";
      std::cout << "prioGroup=" << prioGroup << ", ";
      std::cout << "startTimeInSec=" << startTimeInSec << ", ";

      // std::cout << "pktCnt=" << pktCnt << ", ";
      std::cout << "byteCnt=" << byteCnt << ", ";
      std::cout << "srcAddr=" << srcAddr << ", ";
      std::cout << "dstAddr=" << dstAddr << ", ";
      std::cout << "srcPort=" << srcPort << ", ";
      std::cout << "dstPort=" << dstPort << std::endl;
    }
  };

  struct qlen_freq_entry_t
  {
    std::vector<uint32_t> freq;
    void add(uint32_t qlen)
    {
      uint32_t kb = qlen / 1000;
      if (freq.size() < kb + 1)
      {
        freq.resize(kb + 1);
        freq[kb] = 0;
      }
      freq[kb]++;
    }
  };

  struct rdma_nic_entry_t
  {
    uint32_t idx;
    addr_entry_t addr;
    uint32_t enablePfcMonitor;
    uint32_t kminInKb;
    uint32_t kmaxInKb;
    double pmax;

    void print()
    {
      std::cout << "RDMA NIC ENTRY: ";
      std::cout << "idx=" << idx << ", ";
      std::cout << "network=" << ipv4Address_to_string(addr.network) << ", ";
      std::cout << "prefixLen=" << addr.mask.GetPrefixLength() << ",";
      std::cout << "index=" << ipv4Address_to_string(addr.base) << ", ";
      std::cout << "enablePfcMonitor=" << enablePfcMonitor << ", ";
      std::cout << "kminInKb=" << kminInKb << ", ";
      std::cout << "kmaxInKb=" << kmaxInKb << ", ";
      std::cout << "pmax=" << pmax << ", ";
    }
  };

  struct edge_t
  {
    uint32_t nicIdx;
    bool up;
    uint64_t delayInNs;
    uint64_t bwInBitps;
  };

  struct kv_cache_para_t
  {
    uint32_t idx;
    uint32_t type;
    Ptr<Node> leaderNode;
    NodeContainer followerNodes;
    std::map<Ptr<Node>, std::string> nodeTypeMap;
    NodeContainer leaderNodes;

    uint32_t roundNum;
    uint32_t roundCnt;
    uint32_t completeCnt;
    uint32_t completeNum;
    uint64_t notifySizeInByte;
    uint64_t attentionSizeInByte;
    uint64_t querySizeInByte;
    uint64_t otherTimeInNs;
    uint64_t reduceTimeInNs;
    uint64_t attentionTimeInNs;
    uint32_t state; //  1 2 3 for inca
    kv_cache_para_t()
    {
      roundCnt = 0;
      completeCnt = 0;
    }
  };

  struct global_variable_t
  {
    // std::map<Ptr<Node>, Ipv4Address> svNode2Addr;
    std::map<Ipv4Address, Ptr<Node>> addr2node;
    std::map<Ipv4Address, uint32_t> ip2NodeId;
    std::map<uint32_t, est_entry_t> paraMap;
    std::map<uint32_t, ecn_para_entry_t> ecnParas;
    uint32_t rdmaNicRateInGbps;
    uint32_t defaultPktSizeInByte;
    std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<Ptr<Node>>>> nextHop;
    std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairDelayInNs;
    std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairTxDelayInNs;
    std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairBwInBitps;
    std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairBdpInByte;
    std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairRttInNs;
    uint64_t maxRttInNs;
    uint64_t maxBdpInByte;
    std::string qbbOutputTraceFileName;
    std::string nicInfoOutputFileName;
    // FILE * nicInfoOutputFileHandle;
    std::string paraOutputFileName;
    double requestRate;
    uint64_t flowCount;
    uint64_t totalFlowSizeInByte;
    double simStartTimeInSec;
    double simEndTimeInSec;
    double flowLunchStartTimeInSec;
    double flowLunchEndTimeInSec;
    uint32_t smallFlowCount;
    uint32_t largeFlowCount;
    uint64_t largeFLowThreshInByte;
    uint64_t smallFlowThreshInByte;

    std::string lbsName;
    uint64_t flowletTimoutInUs;
    std::string patternFile;
    std::string workLoadFileName;
    std::string smtFile;
    std::string pitFile;
    std::string pstFile;
    Ptr<Node> srcNode;
    Ptr<Node> dstNode;
    std::vector<flow_entry_t> genFlows;

    std::string configFileName;
    uint32_t pfcPauseTimeInUs;
    bool enableQcn;
    std::map<std::string, std::vector<std::string>> configMap;
    bool enablePfcDynThresh;

    uint32_t intMulti;
    std::string ccMode;
    double pintLogBase;
    int pintBytes; // 2
    uint32_t svCnt;

    uint32_t swCnt;
    NodeContainer svNodes;
    NodeContainer swNodes;
    NodeContainer allNodes;
    // std::string addrFileName;
    std::string topoFileName;
    std::map<uint32_t, CHL_entry_t> channels;
    std::string pfcFileName;
    FILE *pfcFileHandle;
    std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<edge_t>>> edges;
    std::map<uint32_t, std::map<uint32_t, addr_entry_t>> addrs;
    uint32_t alphaShiftInLog;
    std::map<uint32_t, ecn_para_entry_t> ecnParaMap;
    uint32_t mmuSwBufferSizeInMB;
    std::string fctFileName;
    FILE *fctFileHandle;
    nic_para_entry_t nicParas;
    uint32_t pktPayloadSizeInByte;
    bool enableAckHigherPrio;
    std::vector<uint32_t> traceNodeIds;
    NodeContainer traceNodes;
    std::string qbbFileName;
    FILE *qbbFileHandle;
    std::string tfcFileName;
    std::map<uint32_t, std::vector<tfc_entry_t>> tfc;
    double loadRatioShift;
    double loadRatio;
    uint16_t appStartPort;
    uint32_t flowGroupPrio;
    bool enableUnifiedWindow;
    bool enableWindow;

    bool enableQlenMonitor;
    bool enablePfcMonitor;
    bool enableFctMonitor;
    bool enableQbbTrace;
    bool enbaleRateTrace;
    bool enablePfc;
    bool enableTest;
    bool enableIrn;
    bool enableIrnOptimized;
    bool enableProbeTrace;
    bool enablePathDelayTraceForLaps;

    uint64_t qlenMonitorIntervalInNs;
    std::string qlenMonitorFileName;
    std::string rateMonitorFileName;
    FILE *qlenMonitorFileHandle;
    FILE *rateMonitorFileHandle;
    uint64_t qlenMonitorEndTimeInNs;
    uint64_t qlenMonitorStartTimeInNs;

    struct cdf_table *cdfTable;
    uint64_t screenDisplayInNs;

    bool enableFlowCongestTest; // width Is Mbps,
    // kv_cache_para_t incastKvCacheInfo;
    std::map<uint32_t, kv_cache_para_t>
        kvCachePara;
    std::map<uint16_t, kv_cache_para_t *> appPort2kvApp;
    bool enableKvCache;
    std::string inputFileDir;
    std::string outputFileDir;
    std::string kvCacheFileName;
    std::string fileIdx;
    std::string lbsolution;
    std::string topoName;
    std::string kvCacheAlg;
    std::string irnMode;
    uint32_t jobNum;
    uint32_t numOfFinishedJob;
    uint32_t testPktNum;




    global_variable_t() {
      addr2node.clear();
      paraMap.clear();
      ecnParas.clear();
      rdmaNicRateInGbps = 0;
      defaultPktSizeInByte = 0;
      nextHop.clear();
      pairDelayInNs.clear();
      pairTxDelayInNs.clear();
      pairBwInBitps.clear();
      pairBdpInByte.clear();
      pairRttInNs.clear();
      maxRttInNs = 0;
      maxBdpInByte = 0;
      qbbOutputTraceFileName = "";
      nicInfoOutputFileName = "";
      paraOutputFileName = "";
      requestRate = 0;
      flowCount = 0;
      totalFlowSizeInByte = 0;
      simStartTimeInSec = 0;
      simEndTimeInSec = 0;
      flowLunchStartTimeInSec=0;
      flowLunchEndTimeInSec = 0;
      smallFlowCount = 0;
      largeFlowCount = 0;
      largeFLowThreshInByte = 0;
      smallFlowThreshInByte = 0;

      lbsName = "";
      flowletTimoutInUs = 0;
      patternFile = "";
      workLoadFileName = "";

      srcNode = NULL;
      dstNode = NULL;
      genFlows.clear();



      configFileName = "";
      pfcPauseTimeInUs = 0;
      enableQcn = false;
      configMap.clear();
      enablePfcDynThresh = false;

      intMulti = 0;
      ccMode = "";
      pintLogBase = 0;
      pintBytes = 0; // 2
      svCnt = 0;
      swCnt = 0;
      NodeContainer svNodes;
      NodeContainer swNodes;
      NodeContainer allNodes;
      // addrFileName;
      topoFileName = "";
      channels.clear();
      pfcFileName = "";
      pfcFileHandle = NULL;
      edges.clear();
      addrs.clear();
      alphaShiftInLog = 0;
      ecnParaMap.clear();
      mmuSwBufferSizeInMB = 0;
      fctFileName = "";
      fctFileHandle = NULL;
      pktPayloadSizeInByte = 0;
      enableAckHigherPrio = false;
      traceNodeIds.clear();
      NodeContainer traceNodes;
      qbbFileName = "";
      qbbFileHandle = NULL;
      tfcFileName = "";
      tfc.clear();
      loadRatioShift = 0;
      loadRatio = 0;
      appStartPort = 0;
      flowGroupPrio = 0;
      enableUnifiedWindow = false;
      enableWindow = false;
      enableQlenMonitor = false;
      enablePfcMonitor = false;
      enableFctMonitor = false;
      enableQbbTrace = false;
      enablePfc = false;
      enableProbeTrace = false;
      enbaleRateTrace = false;

      qlenMonitorIntervalInNs = 0;
      qlenMonitorFileName = "";
      qlenMonitorFileHandle = NULL;
      qlenMonitorEndTimeInNs = 0;
      qlenMonitorStartTimeInNs = 0;
      rateMonitorFileHandle = NULL;


      cdfTable = NULL;
      screenDisplayInNs = 0;
      simStartTimeInSec = 0.0;
      simEndTimeInSec = 0.0;
      enableFlowCongestTest = false;
      // kv_cache_para_t incastKvCacheInfo;
      kvCachePara.clear();
      appPort2kvApp.clear();
      enableKvCache = false;
      inputFileDir = "";
      outputFileDir = "";
      kvCacheFileName = "";
      fileIdx = "";
      lbsolution = "";
      topoName = "";
      kvCacheAlg = "";
      jobNum = 0;
      numOfFinishedJob = 0;
    }

  };

  // class routeSettings
  //{
  // public:
  //  routeSettings() {}
  //  virtual ~routeSettings() {}

  /* The map between hosts' IP and ID, initial when build topology */
  //  static std::map<Ipv4Address, uint32_t> hostIp2IdMap;
  //  static std::map<Ipv4Address, uint32_t> hostId2IpMap;
  //  static std::map<Ipv4Address, uint32_t> hostIp2SwitchId; // host's IP -> connected Switch's Id
  //};

  template <typename T>
  bool integer_to_bool(T src)
  {
    if (to_string(src) == "0")
    {
      return false;
    }
    else
    {
      return true;
    }
  }

  template <typename T>
  NodeContainer create_nodes(uint32_t n)
  {
    NodeContainer c;
    for (uint32_t i = 0; i < n; i++)
    {
      Ptr<T> sw = CreateObject<T>();
      c.Add(sw);
    }
    return c;
  }

  template <typename T>
  std::string to_data_rate(T src, std::string unit)
  {
    std::string s = to_string(src) + unit;
    return s;
  }

  void add_channel_between_two_nodes(Ptr<Node> firstNode, Ptr<Node> secondNode, PointToPointHelper &p2p);
  uint32_t add_channels(NodeContainer &allNodes, std::map<uint32_t, CHL_entry_t> &CHL);
  void assign_address_to_single_device(Ipv4Address network, Ipv4Mask mask, Ipv4Address base, Ptr<NetDevice> device);
  uint32_t assign_addresses_to_devices(std::map<uint32_t, std::map<uint32_t, addr_entry_t>> &ADDR, NodeContainer &nodes);
  double avg_cdf(struct cdf_table *table);
  /*bool cmp_pitEntry_in_increase_order_of_latency(const PathData *lhs, const PathData *rhs);
  bool cmp_pitEntry_in_increase_order_of_Generation_time(const PathData *lhs, const PathData *rhs);
  bool cmp_pitEntry_in_increase_order_of_congestion_Degree(const PathData *lhs, const PathData *rhs);
  bool cmp_pitEntry_in_decrease_order_of_priority(const PathData *lhs, const PathData *rhs);
  bool cmp_pitEntry_in_increase_order_of_Sent_time(const PathData *lhs, const PathData *rhs);*/
  std::string construct_target_string(uint32_t strLen, std::string c);
  uint32_t create_topology(NodeContainer &switchNodes, NodeContainer &serverNodes, NodeContainer &allNodes, uint32_t switchNum, uint32_t serverNum);
  void free_cdf(struct cdf_table *table);
  double gen_random_cdf(struct cdf_table *table);
  uint32_t hashing_flow_with_5_tuple(Ptr<const Packet> p, const Ipv4Header &header);
  std::string hashing_flow_with_5_tuple_to_string(Ptr<const Packet> packet, const Ipv4Header &header);
  void init_cdf(struct cdf_table *table);
  void install_flow_in_tcp_bulk_on_node_pair(Ptr<Node> srcServerNode, Ptr<Node> dstServerNode, uint16_t port,
                                             uint32_t flowSize, uint32_t packetSize, double startTime, double endTime);
  void install_flows_in_tcp_bulk_on_node_pair(Ptr<Node> srcServerNode, Ptr<Node> dstServerNode, double requestRate,
                                              struct cdf_table *cdfTable, long &flowCount, long &totalFlowSize, double START_TIME,
                                              double END_TIME, double FLOW_LAUNCH_END_TIME, uint16_t &appPort,
                                              uint32_t &smallFlowCount, uint32_t &largeFlowCount);
  void install_tcp_bulk_on_node_pair(Ptr<Node> srcServerNode, Ptr<Node> dstServerNode, uint16_t port, uint32_t flowSize, double START_TIME, double END_TIME);
  void install_tcp_bulk_on_nodes(std::map<uint32_t, std::vector<tfc_entry_t>> &TFC, NodeContainer &nodes,
                                 struct cdf_table *cdfTable, double START_TIME, double END_TIME, double FLOW_LAUNCH_END_TIME,
                                 uint16_t startAppPort, double loadFactor, long &flowCount,
                                 uint32_t &smallFlowCount, uint32_t &largeFlowCount, long &totalFlowSize);
  void install_tcp_test_applications(Ptr<Node> srcNode, Ptr<Node> dstNode, double START_TIME = 0, double END_TIME = 1,
                                     uint16_t appPort = 7777, const uint32_t packetCount = 10, uint32_t intervalInNs = 10000);

  void install_tcp_test_applications_2(Ptr<Node> srcNode, Ptr<Node> dstServer, double START_TIME, double END_TIME, uint16_t appPort, const uint32_t packetCount, uint32_t intervalInNs);
  void install_rdma_client_on_single_node(Ptr<Node> node, flow_entry_t &f);
  void install_rdma_client_on_node(global_variable_t *varMap, uint32_t srcNodeId, uint32_t dstNodeId, uint32_t flownum = 1, uint64_t flowByte = 1000, uint16_t port = 7777);
  void node_install_rdma_application(global_variable_t *varMap);
  void install_udp_echo_applications(Ptr<Node> srcNode, Ptr<Node> dstNode, const uint32_t packetCount);
  std::string integer_vector_to_string_with_range_merge(std::vector<uint32_t> &v);
  double interpolate(double x, double x1, double y1, double x2, double y2);
  void load_cdf(struct cdf_table *table, const char *file_name);
  double poission_gen_interval(double avg_rate);

  uint32_t print_address_for_K_th_device(Ptr<Node> &curNode, uint32_t curIntfIdx);
  uint32_t print_address_for_single_node(Ptr<Node> &curNode);
  uint32_t print_addresses_for_nodes(NodeContainer &allNodes);
  void print_cdf(struct cdf_table *table);
  uint32_t print_connections_for_single_node(Ptr<Node> nodeA);
  uint32_t print_connections_for_nodes(NodeContainer &allNodes);
  void print_EST_to_file(std::string fileName, std::map<uint32_t, est_entry_t> &est);
  void print_probe_info_to_file(std::string outputFileName, std::vector<probeInfoEntry> &m);
  void print_reorder_info_to_file(std::string outputFileName, std::map<std::string, reorder_entry_t> &m_reorderTable);
  void print_TFC(std::map<uint32_t, std::vector<tfc_entry_t>> &TFC, double timeGap);
  double rand_range(double min, double max);
  uint32_t read_ADDR_from_file(std::string addrFile, std::map<uint32_t, std::map<uint32_t, addr_entry_t>> &ADDR);
  uint32_t read_CHL_from_file(std::string chlFile, std::map<uint32_t, CHL_entry_t> &CHL);
  uint32_t read_files_by_line(std::ifstream &fh, std::vector<std::vector<std::string>> &resLines);
  uint32_t read_PIT_from_file(std::string pitFile, std::map<uint32_t, std::map<uint32_t, PathData>> &PIT);
  // uint32_t read_PST_from_file(std::string pstFile, std::map<uint32_t, std::map<PathSelTblKey, pstEntryData>> &PST);
  void Read_pathInfo(global_variable_t *varMap, std::map<Ipv4Address, hostIp2SMT_entry_t> &SMT, std::map<uint32_t, std::map<HostId2PathSeleKey, pstEntryData>> &PST, std::map<uint32_t, std::map<uint32_t, PathData>> &PIT);
  void install_LB_table(global_variable_t *varMap, Ptr<Node> curNode, std::map<Ipv4Address, hostIp2SMT_entry_t> &SMT, std::map<uint32_t, std::map<HostId2PathSeleKey, pstEntryData>> &PST, std::map<uint32_t, std::map<uint32_t, PathData>> &PIT);
  void server_instal_LB_table(global_variable_t *varMap, Ptr<RdmaHw> &rdmaHw, uint32_t nodeId);
  uint32_t read_hostId_PST_Path_from_file(global_variable_t *varMap, std::map<uint32_t, std::map<HostId2PathSeleKey, pstEntryData>> &PST);

  uint32_t read_SMT_from_file(std::string smtFile, std::map<Ipv4Address, hostIp2SMT_entry_t> &SMT);

  uint32_t read_TFC_from_file(std::string trafficFile, std::map<uint32_t, std::vector<tfc_entry_t>> &TFC);
  void read_pattern_from_file(std::string patternFile, std::map<uint32_t, std::vector<tfc_entry_t>> &TFC);
  uint32_t read_TOPO_from_file(std::string topoFile, TOPO_t &TOPO);

  uint32_t read_VMT_from_file(std::string vmtFile, std::map<Ipv4Address, vmt_entry_t> &VMT);
  uint32_t read_flows_from_file(std::string flowFile, std::map<uint32_t, flow_entry_t> &flows);
  void screen_display(uint64_t c);
  PointToPointHelper set_P2P_attribute(uint32_t rate, uint32_t latency, std::string queueType, std::string queueSize);
  uint32_t string_to_integer(const std::string &str);
  Ipv4Address string_to_ipv4Address(std::string src);
  uint32_t get_egress_port_from_pit_entry(PathData *pitEntry);
  std::vector<uint32_t> get_egress_ports_from_pit_entries(std::vector<PathData *> pitEntries);
  void monitor_switch_qlen(global_variable_t *varMap, uint32_t roundIdx);
  // void print_mac_address_for_single_node(Ptr<Node> curNode);
  void monitor_pfc(FILE *os, Ptr<QbbNetDevice> dev, uint32_t type);
  QbbHelper set_QBB_attribute(uint32_t rate, uint32_t latency);
  void add_QBB_channels(global_variable_t *varMap);
  std::vector<std::string> stringSplitWithTargetChar(const std::string &s, char delimiter);
  void config_switch_mmu(global_variable_t *varMap);
  bool stringToBool(const std::string &str);
  std::string boolToString (bool m_value) ;
  void qp_finish(FILE *os, global_variable_t *m, Ptr<RdmaQueuePair> q);
  void parse_default_configures(global_variable_t *varMap);

  void calculate_paths_to_single_server(global_variable_t *varMap, Ptr<Node> host);
  uint32_t calculate_paths_for_servers(global_variable_t *varMap);
  void install_routing_entries(global_variable_t *varMap);
  void install_routing_entries_without_Pathtable(global_variable_t *varMap);
  void print_node_routing_tables(global_variable_t *varMap, uint32_t nodeidx);
  void calculate_bdp_and_rtt(global_variable_t *varMap);
  void set_switch_cc_para(global_variable_t *varMap);

  void config_switch_lb(global_variable_t *varMap);
  void set_QBB_trace(global_variable_t *varMap);
  // void switchportinfoPrint(global_variable_t *varMap, uint32_t nodeId);
  void save_egress_ports_loadinfo(global_variable_t *varMap);
  void save_conga_outinfo(global_variable_t *varMap);
  void save_plb_outinfo(global_variable_t *varMap);
  void save_ecmp_outinfo(global_variable_t *varMap);
  void save_LB_outinfo(global_variable_t *varMap);
  void save_letflow_outinfo(global_variable_t *varMap);
  void save_QpRateChange_outinfo(global_variable_t *varMap);
  void save_Conweave_pathload_outinfo(global_variable_t *varMap);
  void sim_finish(global_variable_t *varMap);
  void save_qpFinshtest_outinfo(global_variable_t *varMap);
  // void print_nic_info(global_variable_t *varMap);
  void generate_rdma_flows_for_node_pair(global_variable_t *varMap);
  void generate_rdma_flows_on_nodes(global_variable_t *varMap);
  void install_rdma_flows_on_nodes(global_variable_t *varMap);
  void create_topology_rdma(global_variable_t *varMap);
  std::map<uint32_t, ecn_para_entry_t> parse_ecn_parameter(std::vector<std::string> &s);
  uint64_t get_nic_rate_In_Gbps(global_variable_t *varMap);
  std::vector<uint32_t> parse_trace_nodes(std::vector<std::string> &nodesIdxes);
  NodeContainer merge_nodes(NodeContainer &first, NodeContainer &second);
  CHL_entry_t parse_channel_entry(std::vector<std::string> &s);
  std::map<uint32_t, CHL_entry_t> parse_channels(std::vector<std::vector<std::string>> &resLines);
  NetDeviceContainer get_all_netdevices_of_a_node(Ptr<Node> node);
  void assign_rdma_addresses_to_node(Ptr<Node> node);
  void assign_addresses(NodeContainer &nodes, std::map<Ipv4Address, Ptr<Node>> &addr2node);
  void record_addr_on_single_node(Ptr<Node> node, std::map<Ipv4Address, Ptr<Node>> &addr2node);
  void assign_node_addresses(global_variable_t *varMap);
  void record_save_addr_on_single_node(Ptr<Node> node, std::map<Ipv4Address, Ptr<Node>> &addr2node, std::map<uint32_t, est_entry_t> &paraMap);
  void config_switch(global_variable_t *varMap);
  void install_rdma_application(global_variable_t *varMap);

  void install_rdma_driver(global_variable_t *varMap);
  void load_default_configures(global_variable_t *varMap);
  void iterate_single_incast_kv_cache_application(global_variable_t *varMap, uint32_t jobIdx);
  void iterate_single_broadcast_kv_cache_application(global_variable_t *varMap, uint32_t jobIdx);
  void qp_finish_kv_cache(FILE *os, global_variable_t *m, Ptr<RdmaQueuePair> q);
  void install_kv_cache_applications(global_variable_t *varMap);
  std::map<uint32_t, kv_cache_para_t> parse_kv_cache_parameter(global_variable_t *varMap);
  void iterate_single_inca_kv_cache_application(global_variable_t *varMap, uint32_t jobIdx);
  void iterate_single_ring_kv_cache_application(global_variable_t *varMap, uint32_t jobIdx);
  flow_entry_t generate_single_rdma_flow(uint32_t flowId, Ptr<Node> srcNode, Ptr<Node> dstNode,
                                         uint32_t port, uint32_t flowSize, double START_TIME, uint32_t flowPg,
                                         uint64_t winInByte, uint64_t rttInNs);

  void install_rdma_test_application(global_variable_t *varMap, uint32_t srcNodeId, uint32_t dstNodeId, uint32_t port, uint32_t flowSize, double startTime);
  // struct reorderDistEntry {
  //   std::vector<uint32_t> freq;
  //   uint64_t startTime;
  //   uint64_t lastTime;
  //   uint64_t maxValue;
  //   uint64_t maxIndex;
  //   uint32_t counter;
  //   uint64_t size;
  //   reorderDistEntry(){
  //     freq.clear();
  //     maxIndex = 0;
  //     maxValue = 0;
  //     counter = 0;
  //     size = 0;
  //   }
  //   bool operator==(const reorderDistEntry& other) const {
  //     return maxValue == other.maxValue && maxIndex == other.maxIndex && counter == other.counter&& size == other.size && startTime == other.startTime;
  //   }
  //   reorderDistEntry(uint32_t seq, uint32_t s){
  //     freq.clear();
  //     maxIndex = 1;
  //     maxValue = seq;
  //     counter = 1;
  //     size = s;
  //     startTime = Now().GetNanoSeconds();
  //     lastTime = Now().GetNanoSeconds();
  //   }
  //   void Print(std::ostream &os){
  //     os << std::setiosflags (std::ios::left) << std::setw (7) << counter;
  //     os << std::setiosflags (std::ios::left) << std::setw (10) << size;
  //     double avgPktSize = counter == 0 ? 0 : 1.0*size/counter;
  //     os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(DEFAULT_PRECISION_FOR_FLOAT_TO_STRING) << avgPktSize;
  //     double avgPktGap = counter <= 1 ? 0 : 1.0*(lastTime-startTime)/(counter-1)/1000;
  //     os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(DEFAULT_PRECISION_FOR_FLOAT_TO_STRING) << avgPktGap;
  //     double ratio = freq.size() == 0 ? 0 : 1.0*std::accumulate(freq.begin(), freq.end(), 0)/counter;
  //     os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(DEFAULT_PRECISION_FOR_FLOAT_TO_STRING) << ratio;
  //     os << vector2string<uint32_t> (freq, ", ");
  //     os << std::endl;
  //   }

  //   std::string ToString(){
  //       std::ostringstream os;
  //       os << std::setiosflags (std::ios::left) << std::setw (7) << counter;
  //       os << std::setiosflags (std::ios::left) << std::setw (10) << size;
  //       double avgPktSize = counter == 0 ? 0 : 1.0*size/counter;
  //       os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(DEFAULT_PRECISION_FOR_FLOAT_TO_STRING) << avgPktSize;
  //       double avgPktGap = counter <= 1 ? 0 : 1.0*(lastTime-startTime)/(counter-1)/1000;
  //       os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(DEFAULT_PRECISION_FOR_FLOAT_TO_STRING) << avgPktGap;
  //       double ratio = freq.size() == 0 ? 0 : 1.0*std::accumulate(freq.begin(), freq.end(), 0)/counter;
  //       os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(DEFAULT_PRECISION_FOR_FLOAT_TO_STRING) << ratio;
  //       os << vector2string<uint32_t> (freq, ", ");
  //       return os.str();
  //   }

  // };

  void SinkRx(Ptr<const Packet> p, const Address &from, const Address &local, const SeqTsSizeHeader &hdr);
  // void tcpSocketBaseBxCb (std::map<std::string, reorderDistEntry> * reorderDistTbl, std::string fid, Ptr<const Packet> p, const TcpHeader& tcpHdr,  Ptr<const TcpSocketBase> skt);

  // void addTcpSocketBaseBxCb(std::map<std::string, reorderDistEntry> * reorderDistTbl);

  struct QueueLengthEntry
  {
    std::vector<std::pair<uint32_t, uint32_t>> len;
    uint32_t maxLen;
    uint32_t round;
    uint64_t totalLen;
    QueueLengthEntry()
    {
      len.clear();
      maxLen = 0;
      round = 0;
      totalLen = 0;
    }
    bool operator==(const QueueLengthEntry &other) const
    {
      return maxLen == other.maxLen && len == other.len && round == other.round;
    }
    QueueLengthEntry(uint32_t l)
    {
      len.resize(1, std::make_pair(1, l));
      maxLen = l;
      round = 1;
      totalLen = l;
    }

    void Print(std::ostream &os)
    {
      double avgLen = 1.0 * totalLen / round;
      os << std::setiosflags(std::ios::left) << std::setw(7) << round;
      os << std::setiosflags(std::ios::left) << std::setw(7) << std::fixed << std::setprecision(2) << avgLen;
      os << std::setiosflags(std::ios::left) << std::setw(7) << maxLen;
      os << PairVector2string(len, ", ");
      os << std::endl;
    }
    std::string ToString()
    {
      std::ostringstream oss;
      double avgLen = 1.0 * totalLen / round;
      oss << std::setiosflags(std::ios::left) << std::setw(7) << round;
      oss << std::setiosflags(std::ios::left) << std::setw(7) << std::fixed << std::setprecision(2) << avgLen;
      oss << std::setiosflags(std::ios::left) << std::setw(7) << maxLen;
      oss << PairVector2string(len, ", ");
      return oss.str();
    }
   };

  void PrintNodeQlen(std::vector<std::vector<QueueLengthEntry>> *qlenTbl, NodeContainer nodes, uint64_t interval = 100000, uint32_t round = 1);
  void SaveNodeQlen(std::string file, std::vector<std::vector<QueueLengthEntry>> &qlenTbl, uint64_t intervalInNs = 1000000);
  void SaveReorderDregree(std::string file, std::map<std::string, reorderDistEntry> &reorderDistTbl);

  struct CommunicationPatternEntry
  {
    std::string srcNodeName;
    std::string dstNodeName;
    double loadAdjustRate;
    CommunicationPatternEntry()
    {
      loadAdjustRate = 1;
    }
    bool operator==(const CommunicationPatternEntry &other) const
    {
      return srcNodeName == other.srcNodeName && dstNodeName == other.dstNodeName;
    }
    CommunicationPatternEntry(std::string src, std::string dst, double lr)
    {
      loadAdjustRate = lr;
      srcNodeName = src;
      dstNodeName = dst;
    }

    void Print(std::ostream &os)
    {
      os << std::setiosflags(std::ios::left) << std::setw(10) << srcNodeName;
      os << std::setiosflags(std::ios::left) << std::setw(10) << dstNodeName;
      os << std::setiosflags(std::ios::left) << std::setw(8) << loadAdjustRate;
      os << std::endl;
    }
    std::string ToString()
    {
      std::ostringstream oss;
      oss << std::setiosflags(std::ios::left) << std::setw(10) << srcNodeName;
      oss << std::setiosflags(std::ios::left) << std::setw(10) << dstNodeName;
      oss << std::setiosflags(std::ios::left) << std::setw(8) << loadAdjustRate;
      return oss.str();
    }
  };

  std::vector<CommunicationPatternEntry> read_pattern_file(std::string trafficFile);
  struct cdf_table *read_workload_file(std::string file);
  void install_tcp_bulk_apps(std::vector<CommunicationPatternEntry> &pEntries, struct cdf_table *cdfTable,
                             double START_TIME, double END_TIME, double FLOW_LAUNCH_END_TIME, uint16_t startAppPort, double loadFactor);

  void SaveFlowInfo(std::string file, std::map<std::string, flowInfo> &flowTable);
  void addTcpSocketBaseBxCb(Ptr<PacketSink> sink, std::string fid, std::map<std::string, reorderDistEntry> *reorderDistTbl);
  void DisplayNodeQlen(std::vector<std::vector<QueueLengthEntry>> &qlenTbl, uint64_t intervalInNs);

  void install_kv_cache_applications(global_variable_t *varMap);
  void monitor_special_port_qlen(global_variable_t *varMap, uint32_t nodeId, uint32_t portId, uint32_t roundIdx);

  void install_flows_in_tcp_bulk_on_node_pair_with_flow_classification(Ptr<Node> srcServerNode, Ptr<Node> dstServerNode,
                                                                       double requestRate, struct cdf_table *cdfTable, long &flowCount, long &totalFlowSize,
                                                                       double START_TIME, double END_TIME, double FLOW_LAUNCH_END_TIME,
                                                                       uint16_t &appPort, uint32_t &smallFlowCount, uint32_t &largeFlowCount, std::map<uint16_t, uint32_t> &port2flowSize);

  void install_tcp_bulk_apps_with_flow_classification(std::vector<CommunicationPatternEntry> &pEntries, struct cdf_table *cdfTable,
                                                      double START_TIME, double END_TIME, double FLOW_LAUNCH_END_TIME,
                                                      uint16_t startAppPort, double loadFactor, std::map<uint16_t, uint32_t> &port2flowSize);

  std::map<uint16_t, uint32_t> PickTopFlows(const std::map<uint16_t, uint32_t> &port2flowSize, double ratio);
  std::vector<PathData> load_PIT_from_file(std::string pitFile);
  std::vector<std::vector<std::string> > read_content_as_string(std::ifstream &fh);
  void cal_metadata_on_PIT_from_laps(global_variable_t *varMap, std::vector<PathData> &paths);
  void install_routing_entries_based_on_single_pit_entry_for_laps(global_variable_t *varMap, PathData &pit);
  std::vector <pstEntryData> load_PST_from_file(std::string pstFile);
  void install_routing_entries_based_on_single_pst_entry_for_laps(global_variable_t *varMap, pstEntryData &pst);
  std::map<Ipv4Address, uint32_t> Calulate_SMT_for_laps(NodeContainer nodes);
  void install_routing_entries_based_on_single_smt_entry_for_laps(NodeContainer nodes, std::map<Ipv4Address, uint32_t> &ip2nodeId);
  void install_routing_entries_for_laps(global_variable_t *varMap) ;
  void print_flow_rate_record(global_variable_t *varMap);



}
#endif /* USERDEFINEDFUNCTION_H */
