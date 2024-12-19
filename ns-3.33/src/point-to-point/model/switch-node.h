#ifndef SWITCH_NODE_H
#define SWITCH_NODE_H

#include <unordered_map>
#include <ns3/node.h>
#include "qbb-net-device.h"
#include "switch-mmu.h"
#include "pint.h"
#include "ns3/userdefinedfunction.h"
#include "ns3/rdma-hw.h"

namespace ns3
{

  class Packet;

  enum LB_Solution
  {
    LB_ECMP = 0,
    LB_RPS = 1,
    LB_DRILL = 2,
    LB_LETFLOW = 3,
    LB_DEFLOW = 4,
    LB_CONGA = 5,
    LB_LAPS = 6,
    LB_RRS = 7,
    LB_NONE = 8
  };

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
    int GetOutDev(Ptr<const Packet>, CustomHeader &ch);
    void SendToDev(Ptr<Packet> p, CustomHeader &ch);
    static uint32_t EcmpHash(const uint8_t *key, size_t len, uint32_t seed);
    void CheckAndSendPfc(uint32_t inDev, uint32_t qIndex);
    void CheckAndSendResume(uint32_t inDev, uint32_t qIndex);
    uint32_t m_d; // drill LB
    // Flowlet Table
    std::map<std::string, LetFlowFlowletInfo> m_flowletTable;
    Time m_flowletTimeout;

  public:
    Ptr<SwitchMmu> m_mmu;
    static TypeId GetTypeId(void);
    SwitchNode();
    void SetEcmpSeed(uint32_t seed);
    void SetSeed(uint32_t seed);
    void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
    void ClearTable();
    bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);
    void SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);
    void SetFlowletTimeout(Time timeout);

    static std::map<uint32_t, std::map<uint32_t, uint32_t>> m_rpsPortInf;                      // map from nodeid to port to packetCount (RPS select port)
    static std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, uint32_t>>> m_ecmpPortInf; // map from nodeid to port to flowid,flowidCount (ECMP select port)
    static std::map<uint32_t, std::map<uint32_t, uint32_t>> m_rrsPortInf;                      // map from nodeid to port to packetCount (RRS select port)
    // for approximate calc in PINT
    int logres_shift(int b, int l);
    int log2apprx(int x, int b, int m, int l); // given x of at most b bits, use most significant m bits of x, calc the result in l bits
  };

} /* namespace ns3 */

#endif /* SWITCH_NODE_H */
