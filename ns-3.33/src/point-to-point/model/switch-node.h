#ifndef SWITCH_NODE_H
#define SWITCH_NODE_H

#include <unordered_map>
#include <ns3/node.h>
#include "qbb-net-device.h"
#include "switch-mmu.h"
#include "pint.h"
// #include "ns3/userdefinedfunction.h"
#include "ns3/rdma-hw.h"

namespace ns3
{

  class Packet;

  

  template <typename T>
  std::string ports_to_string(std::vector<T> src)
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
        std::string curStr = std::to_string(src[i]);
        str = str + curStr + ", ";
      }
      std::string curStr = std::to_string(src[vecSize - 1]);
      str = str + curStr + "]";
      return str;
    }
  }

  struct CandidatePortEntry
  {
    std::vector<uint32_t> ports;
    CandidatePortEntry()
    {
      ports.clear();
    }
    bool operator==(const CandidatePortEntry &other) const
    {
      if (ports.size() != other.ports.size())
      {
        return false;
      }
      return std::equal(ports.begin(), ports.end(), other.ports.begin());
    }
    CandidatePortEntry(std::vector<uint32_t> &p)
    {
      ports = p;
    }

    void Print(std::ostream &os)
    {
      os << ports_to_string(ports);
      os << std::endl;
    }
    std::string ToString()
    {
      std::ostringstream oss;
      oss << std::setiosflags(std::ios::left) << std::setw(30) << ports_to_string(ports);
      return oss.str();
    }
  };
  struct DrillEntryInfo : public CandidatePortEntry
  {
    std::map<uint32_t, uint32_t> m_previousBestQueueMap; // PORT->QUEUE
  };
  struct RrsEntryInfo : public CandidatePortEntry
  {
    uint32_t current_index; // port_index
  };
  struct LetFlowFlowletInfo
  {
    uint32_t port;
    Time activeTime;
    uint64_t nPackets;
  };
  struct CongaFlowlet
  {
    Time activeTime;    // to check creating a new flowlet
    Time activatedTime; // start time of new flowlet
    uint32_t PathId;    // current pathId
    uint32_t nPackets;  // for debugging
  };
  const uint32_t CONGA_NULL = UINT32_MAX;

  struct FeedbackInfo
  {
    uint32_t _ce;
    Time _updateTime;
  };

  struct OutpathInfo
  {
    uint32_t _ce;
    Time _updateTime;
  };

  class CongaTag : public Tag
  {
  public:
    CongaTag();
    ~CongaTag();
    static TypeId GetTypeId(void);
    void SetPathId(uint32_t pathId);
    uint32_t GetPathId(void) const;
    void SetCe(uint32_t ce);
    uint32_t GetCe(void) const;
    void SetFbPathId(uint32_t fbPathId);
    uint32_t GetFbPathId(void) const;
    void SetFbMetric(uint32_t fbMetric);
    uint32_t GetFbMetric(void) const;
    void SetHopCount(uint32_t hopCount);
    uint32_t GetHopCount(void) const;
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream &os) const;

  private:
    uint32_t m_pathId;   // forward
    uint32_t m_ce;       // forward
    uint32_t m_hopCount; // hopCount to get outPort
    uint32_t m_fbPathId; // feedback
    uint32_t m_fbMetric; // feedback
  };
  // std::map<uint32_t, uint32_t> SwitchNode::m_rpsPortInf;
  class SwitchNode : public Node
  {
    static const uint32_t pCnt = 257; // Number of ports used
    static const uint32_t qCnt = 8;   // Number of queues/priorities used
    uint32_t m_ecmpSeed;
    unsigned int m_Seed;
    std::unordered_map<uint32_t, std::vector<int>> m_rtTable; // map from ip address (u32) to possible port (index of dev,ecmp��drill��rps)
    //  monitor of PFC
    uint32_t m_bytes[pCnt][pCnt][qCnt]; // m_bytes[inDev][outDev][qidx] is the bytes from inDev enqueued for outDev at qidx

    uint64_t m_txBytes[pCnt]; // counter of tx bytes

    uint32_t m_lastPktSize[pCnt];
    uint64_t m_lastPktTs[pCnt]; // ns
    double m_u[pCnt];

    LB_Solution m_lbSolution;
    std::map<Ipv4Address, CandidatePortEntry> m_ecmpRouteTable;
    std::map<Ipv4Address, CandidatePortEntry> m_rpsRouteTable;
    std::map<Ipv4Address, CandidatePortEntry> m_letflowRouteTable;
    std::map<Ipv4Address, RrsEntryInfo> m_rrsRouteTable;
    std::map<Ipv4Address, DrillEntryInfo> m_drillRouteTable;
    uint32_t GetHashValueFromCustomHeader(const CustomHeader &ch);
    std::string GetStringHashValueFromCustomHeader(const CustomHeader &ch);
    CandidatePortEntry GetEcmpRouteEntry(const Ipv4Address dip) const;
    CandidatePortEntry GetRpsRouteEntry(const Ipv4Address dip) const;
    RrsEntryInfo GetRrsRouteEntry(const Ipv4Address dip) const;
    DrillEntryInfo GetDrillRouteEntry(const Ipv4Address dip) const;
    CandidatePortEntry GetLetFlowRouteEntry(const Ipv4Address dip) const;
    bool reach_the_last_hop_of_path_tag(CongaTag congaTag);
    uint32_t CalculateQueueLength(uint32_t interface);
    uint32_t GetDrillEgressPort(const Ipv4Address dip);
    uint32_t GetLetFlowEgressPort(const Ipv4Address dip, std::string flowId);
    uint32_t randomSelectionFromTime();

  protected:
    bool m_ecnEnabled;
    CongestionControlMode m_ccMode;
    uint64_t m_maxRtt;
    uint32_t m_ackHighPrio; // set high priority for ACK/NACK

  private:
    int GetOutDev(Ptr<Packet>, CustomHeader &ch);
    void SendToDev(Ptr<Packet> p, CustomHeader &ch);
    static uint32_t EcmpHash(const uint8_t *key, size_t len, uint32_t seed);
    void CheckAndSendPfc(uint32_t inDev, uint32_t qIndex);
    void CheckAndSendResume(uint32_t inDev, uint32_t qIndex);
    uint32_t m_d; // drill LB
    // Flowlet Table
    std::map<std::string, LetFlowFlowletInfo> m_flowletTable;
    Time m_flowletTimeout;
    bool m_isToR;         // is ToR (leaf)
    uint32_t m_switch_id; // switch's nodeID
    // conga
    Time m_dreTime;         // dre algorithm (e.g., 200us) timegap
    double m_alpha;         // dre algorithm (e.g., 0.2)
    Time m_agingTime;       // aging time (e.g., 10ms)
    uint32_t m_quantizeBit; // quantizing (2**X) param (e.g., X=3)
    // local
    std::map<uint32_t, uint32_t> m_DreMap;                     // outPort -> DRE (at SrcToR)
    std::map<std::string, CongaFlowlet *> m_congaflowletTable; // QpKey -> Flowlet (at SrcToR)

  public:
    Ptr<SwitchMmu> m_mmu;
    static TypeId GetTypeId(void);
    SwitchNode();
    void SetEcmpSeed(uint32_t seed);
    void SetSeed(uint32_t seed);
    void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
    void ClearTable();
    bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);
    void DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev, uint32_t qIndex);
    void SendToDevContinue(Ptr<Packet> p, CustomHeader &ch);
    void SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);
    void SetFlowletTimeout(Time timeout);
    struct FlowPortInfo
    {
      uint32_t Packetcount;
      uint32_t Packetsize;
    };
    static std::map<uint32_t, std::map<uint32_t, uint32_t>> m_rpsPortInf;                      // map from nodeid to port to packetCount (RPS select port)
    static std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, uint32_t>>> m_ecmpPortInf; // map from nodeid to port to flowid,flowidCount (ECMP select port)
    static std::map<uint32_t, std::map<uint32_t, uint32_t>> m_rrsPortInf;                      // map from nodeid to port to packetCount (RRS select port)
    static std::map<uint32_t, std::map<uint32_t, FlowPortInfo>> m_PortInf;
    static std::map<uint32_t,std::map<uint64_t, std::string>>congaoutinfo;
    static uint32_t GetOutPortFromPath(const uint32_t &path, const uint32_t &hopCount); // decode outPort from path, given a hop's order
    bool GetIsToRSwitch();
    uint32_t GetSwitchId();
    static void SetOutPortToPath(uint32_t &path, const uint32_t &hopCount, const uint32_t &outPort); // encode outPort to path,,hopcount <=sizeof(path)=4
    uint32_t GetNormalEcmpPort(Ptr<Packet> p, CustomHeader &ch);
    uint32_t GetCongaBestPath(HostId2PathSeleKey forwarPstKey, uint32_t nSample);
    static uint32_t nFlowletTimeout;
    // map from nodeid to port to packetCount��packetsize
    // for approximate calc in PINT
    int logres_shift(int b, int l);
    int log2apprx(int x, int b, int m, int l); // given x of at most b bits, use most significant m bits of x, calc the result in l bits
    /* main function :GetEgressPort */
    uint32_t GetCongaEgressPort(Ptr<Packet> p, CustomHeader &ch);
    uint32_t UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort);
    uint32_t QuantizingX(uint32_t outPort, uint32_t X); // X is bytes here and we quantizing it to 0 - 2^Q
    uint32_t GetBestPath(uint32_t dstTorId, uint32_t nSample);
    std::map<Ipv4Address, CandidatePortEntry> m_congaIp2ports;

    static RoutePath routePath;
    /* SET functions */
    void SetConstants(Time dreTime, Time agingTime, uint32_t quantizeBit, double alpha); // not set flowlet time
    void SetSwitchInfo(bool isToR, uint32_t switch_id);
    void SetLinkCapacity(uint32_t outPort, uint64_t bitRate);

    // periodic events
    EventId m_dreEvent;
    EventId m_agingEvent;
    void DreEvent();
    void AgingEvent();
    


    // topological info (should be initialized in the beginning)
    std::map<uint32_t, std::set<uint32_t>> m_congaRoutingTable;                // routing table (ToRId -> pathId) (stable)
    std::map<uint32_t, std::map<uint32_t, FeedbackInfo>> m_congaFromLeafTable; // ToRId -> <pathId -> FeedbackInfo> (aged)
    std::map<uint32_t, std::map<uint32_t, OutpathInfo>> m_congaToLeafTable;    // ToRId -> <pathId -> OutpathInfo> (aged)
    std::map<uint32_t, uint64_t> m_outPort2BitRateMap;                         // outPort -> link bitrate (bps) (stable)
  };

} /* namespace ns3 */
#endif /* SWITCH_NODE_H */
