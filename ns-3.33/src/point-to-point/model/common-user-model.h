#ifndef __COMMON_USER_MODEL_H__
#define __COMMON_USER_MODEL_H__

#include <iostream>
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <list>
#include <numeric>
#include <sstream>
#include <string>
#include "ns3/string.h"
#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/pause-header.h"
#include <ns3/custom-header.h>
#include "ns3/flow-id-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/ipv4-smartflow-tag.h"
#include "ns3/random-variable-stream.h"
namespace ns3
{

#define PARSE_FIVE_TUPLE(ch)                                                    \
    DEPARSE_FIVE_TUPLE(ipv4Address2string(Ipv4Address(ch.sip)),                 \
                       std::to_string(ch.udp.sport),                            \
                       ipv4Address2string(Ipv4Address(ch.dip)),                 \
                       std::to_string(ch.udp.dport), std::to_string(ch.l3Prot), \
                       std::to_string(ch.udp.seq), std::to_string(ch.GetIpv4EcnBits()))
#define PARSE_REVERSE_FIVE_TUPLE(ch)                                            \
    DEPARSE_FIVE_TUPLE(ipv4Address2string(Ipv4Address(ch.dip)),                 \
                       std::to_string(ch.udp.dport),                            \
                       ipv4Address2string(Ipv4Address(ch.sip)),                 \
                       std::to_string(ch.udp.sport), std::to_string(ch.l3Prot), \
                       std::to_string(ch.udp.seq), std::to_string(ch.GetIpv4EcnBits()))
#define DEPARSE_FIVE_TUPLE(sip, sport, dip, dport, protocol, seq, ecn)                        \
    sip << "(" << sport << ")," << dip << "(" << dport << ")[" << protocol << "],SEQ:" << seq \
        << ",ECN:" << ecn << ","

#define ENABLE_CCMODE_TEST false
#define ENABLE_LOSS_PACKET_TEST false
    enum LB_Solution
    {
        LB_ECMP = 0,
        LB_RPS = 1,
        LB_DRILL = 2,
        LB_LETFLOW = 3,
        LB_DEFLOW = 4,
        LB_CONGA = 5,
        LB_LAPS = 6,
        LB_E2ELAPS = 11,
        LB_RRS = 7,
        LB_CONWEAVE = 8,
        LB_PLB = 9,
        LB_NONE = 10

    };

    template <typename T>
    size_t GetHashValue(std::vector<T> &key)
    {
        std::string str = "";
        for (size_t i = 0; i < key.size(); i++)
        {
            str = str + "#" + std::to_string(key[i]);
        }
        std::hash<std::string> hash_fn;
        return hash_fn(str);
    }

    template <typename T1, typename T2>
    std::string ListToString(const std::list<std::pair<T1, T2>> &d)
    {
        std::ostringstream oss;
        for (const auto &item : d)
        {
            oss << "(" << item.first << ", " << item.second << "), ";
        }
        std::string result = oss.str();
        if (!result.empty())
        {
            result.pop_back(); // 移除最后一个空格
            result.pop_back(); // 移除最后一个逗号
        }
        else
        {
            result = "()";
        }
        return result;
    }

    struct RecordCcmodeOutEntry
    {
        uint64_t currdatarate = 0;
        uint64_t nextdatarate = 0;
        uint32_t m_cnt_cnpByEcn = 0;
        uint32_t m_cnt_cnpByOoo = 0;
        uint32_t m_cnt_Cnp = 0;
        bool IsCnp;
    };

    struct LostPacketEntry
    {
        uint64_t snd_nxt = 0;
        uint64_t snd_una = 0;
        uint64_t RTO = 0;
        uint32_t currlossPacketSeq = 0;
        uint32_t lostNum = 0;
        bool IsRT;
    };

    struct RecordFlowRateEntry_t
    {
        uint64_t rateInMbps = 0;
        uint64_t startTimeInNs = 0;
        uint64_t durationInNs = 0;
        RecordFlowRateEntry_t(uint64_t rateInMbps, uint64_t startTimeInNs, uint64_t durationInNs) :
        rateInMbps(rateInMbps), startTimeInNs(startTimeInNs), durationInNs(durationInNs) {}
        std::string to_string()
        {
            std::string str = "[" + std::to_string(rateInMbps) + ", ";
            str += std::to_string(startTimeInNs) + ", ";
            str += std::to_string(durationInNs) + "]";
            return str;
        }
    };

    struct RecordPathDelayEntry_t
    {
        uint64_t delayInNs = 0;
        uint64_t startTimeInNs = 0;
        uint64_t durationInNs = 0;
        RecordPathDelayEntry_t(uint64_t delayInNs, uint64_t startTimeInNs, uint64_t durationInNs) :
        delayInNs(delayInNs), startTimeInNs(startTimeInNs), durationInNs(durationInNs){}
        std::string to_string()
        {
            std::string str = "[" + std::to_string(delayInNs) + ", ";
            str += std::to_string(startTimeInNs) + ", ";
            str += std::to_string(durationInNs) + "]";
            return str;
        }

        // std::vector<std::pair<uint64_t, uint64_t>> delays;
        // std::string to_string()
        // {
        //     std::string str = std::to_string(pid) + " ";
        //     for (size_t i = 0; i < delays.size(); i++)
        //     {
        //         str += std::to_string(delays[i].first) + ",";
        //         str += std::to_string(delays[i].second) + " ";
        //     }
        //     str += std::to_string(curRateInMbps) + " ";
        //     str += std::to_string(tgtRateInMbps) + " ";
        //     str += reason + " ";
        //     str += std::to_string(incStage);
        //     return str;
        // }
    };

	struct OutStandingDataEntry {
		uint32_t flow_id;
		uint32_t seq;
		uint16_t size;
		std::string to_string() {
			return "[" + std::to_string(flow_id) + ", " + std::to_string(seq) + ", " + std::to_string(size) + "]";
		}
        OutStandingDataEntry(uint32_t flow_id, uint32_t seq, uint16_t size) : flow_id(flow_id), seq(seq), size(size) {}

	};



    bool IsVectorReverse(const std::vector<uint32_t>& vec1, const std::vector<uint32_t>& vec2);


    struct HostId2PathSeleKey
    {
        uint32_t selfHostId;
        uint32_t dstHostId;
        HostId2PathSeleKey() : selfHostId(0), dstHostId(0) {}
        HostId2PathSeleKey(uint32_t &self, uint32_t &dest) : selfHostId(self), dstHostId(dest) {}
        bool operator<(const HostId2PathSeleKey &other) const
        {
            if (this->selfHostId < other.selfHostId)
            {
                return true;
            }

            if (this->selfHostId == other.selfHostId)
            {
                return (this->dstHostId < other.dstHostId);
            }
            return false;
        }

        bool operator==(const HostId2PathSeleKey &other) const
        {
            return this->selfHostId == other.selfHostId && this->dstHostId == other.dstHostId;
        }

        void print()
        {
            std::cout << "PST_KEY: selfHostId=" << selfHostId << ", ";
            std::cout << "dstHostId=" << dstHostId << std::endl;
        }
        std::string to_string()
        {
            std::string str = std::to_string(selfHostId) + "->" + std::to_string(dstHostId);
            return str;
        }
    };

    struct pstEntryData
    {
        HostId2PathSeleKey key;
        uint32_t pathNum;
        uint32_t lastSelectedPathIdx;
        uint32_t lastPiggybackPathIdx;
        uint32_t lastSelectedTimeInNs;
        uint32_t highestPriorityPathIdx;
        uint32_t baseRTTInNs; // link_delay ns*2*hopcount
        // uint32_t smallestLatencyInNs;
        std::vector<uint32_t> paths;
        void print()
        {
            std::cout << "PST_DATA: pathNum=" << pathNum << ", ";
            std::cout << "lastSelectedIdx=" << lastSelectedPathIdx << ", ";
            std::cout << "lastPiggybackPathIdx=" << lastPiggybackPathIdx << ", ";
            std::cout << "highestPriorityIdx=" << highestPriorityPathIdx << ", ";
            std::cout << "lastSelectedTimeInNs=" << lastSelectedTimeInNs << ", ";
            std::cout << "Paths=" << vectorTostring(paths) << std::endl;
        }
        pstEntryData()
        {
            pathNum = 0;
            lastSelectedPathIdx = 0;
            lastPiggybackPathIdx = 0;
            lastSelectedTimeInNs = 0;
            highestPriorityPathIdx = 0;
            baseRTTInNs = 0;
            paths.clear();
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
    struct reorder_entry_t
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
            std::cout << "SeqNums=" << vectorTostring<uint32_t>(seqs);
            std::cout << std::endl;
        }
    };

    /*struct PathSelTblKey
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
    };*/

 
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
    };
    struct hostIp2SMT_entry_t
    {
        Ipv4Address hostIp;
        uint32_t torId;
        uint32_t hostId;
        void print()
        {
            std::cout << "hostIp2SMT: TORID=" << torId << " hostId:" << hostId << std::endl;
        }
    };

    std::string GetStringHashValueFromCustomHeader(const CustomHeader &ch);
    std::string ipv4Address2string(Ipv4Address addr);
    struct pathload
    {
        uint32_t pid = 0;
        uint64_t lastsize = 0;
        uint64_t currsize = 0;
    };
    class RoutePath
    {

    public:
        RoutePath();
        ~RoutePath();
        std::map<std::string, flet_entry_t> m_fletTbl;
        std::map<std::string, rbn_entry_t> m_rbnTbl;

        std::map<Ipv4Address, hostIp2SMT_entry_t> m_vmVtepMapTbl; // SMT
        std::map<HostId2PathSeleKey, pstEntryData> m_pathSelTbl;  // PDT
        std::map<uint32_t, PathData> m_nexthopSelTbl;             // PIT
        std::map<HostId2PathSeleKey, pdt_entry_t> m_pathDecTbl;   //
        // std::map<HostId2PathSeleKey, std::map<uint32_t, pathload>> m_recordPath; // timegap->pid->sendpacketsize
        std::vector<PathData *> batch_lookup_PIT(std::vector<uint32_t> &pids);
        uint32_t install_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &vmt);
        void set_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &vmVtepMapTbl);
        hostIp2SMT_entry_t *lookup_SMT(const Ipv4Address &serverAddr);
        uint32_t install_PST(std::map<HostId2PathSeleKey, pstEntryData> &pst);
        void set_PST(std::map<HostId2PathSeleKey, pstEntryData> &pathSelTbl);
        pstEntryData *lookup_PST(HostId2PathSeleKey &pstKey);
        uint32_t install_PIT(std::map<uint32_t, PathData> &pit);
        void set_PIT(std::map<uint32_t, PathData> &nexthopSelTbl);
        PathData *lookup_PIT(uint32_t pieKey);
        uint32_t print_PIT();
        // uint32_t print_PST();
        uint32_t print_SMT();
        uint32_t m_switch_id;
    };

    class routeSettings
    {
    public:
        routeSettings() {}
        virtual ~routeSettings() {}
        static std::map<Ipv4Address, uint32_t> hostIp2IdMap;
        static std::map<uint32_t, Ipv4Address> hostId2IpMap;
        static std::map<Ipv4Address, uint32_t> hostIp2SwitchId;
        static std::map<uint32_t, std::vector<Ipv4Address>> ToRSwitchId2hostIp;
        static std::map<Ipv4Address, uint32_t> ip2IdMap;
    };

//******PLB*******//
#define IDLE_REHASH_ROUNDS 3
#define PLB_REHASH_ROUNDS 12
#define PLB_SUSPEND_RTO_SEC 60

    struct QpRecordEntry
    {
        static uint32_t installFlowCnt;
        static uint32_t finishFlowCnt;
        static uint32_t execFlowCnt;
        static std::map<uint32_t, uint64_t> lastNonePktsTime;
        uint32_t flowId;
        uint32_t sendSizeInbyte = 0;
        uint32_t receSizeInbyte = 0;
        uint32_t sendAckInbyte = 0;
        uint32_t receAckInbyte = 0;
        uint32_t sendPacketNum = 0;
        uint32_t recePacketNum = 0;
        uint32_t sendAckPacketNum = 0;
        uint32_t receAckPacketNum = 0;
        uint32_t flowsize;
        uint64_t installTime = 0;
        int64_t lastVistTime = -1;
        uint32_t srcNodeId = 0;
        uint32_t dstNodeId = 0;
        uint32_t windowSize = 0;
        std::string hdrInfo = "None"; 
        bool isFinished = false;
        std::string pauseReason = "None";
        uint32_t pauseCount = 0;
        uint64_t initialRate = 0;
        uint64_t nxtAvailTime = 0;
        uint64_t lastSelectedTime = 0;
        uint64_t snd_una = 0;
        uint64_t snd_nxt = 0;
        int64_t lastPfcPauseTime = -1;
        uint64_t pfcDuration = 0;
        uint64_t finishTime = 0;

        static std::string get_title()
        {
            uint32_t n_blackspaces = 8;
            std::string spaces = "";
            for (uint32_t i = 0; i < n_blackspaces; i++)
            {
                spaces += " ";
            } 
            std::ostringstream oss;
            oss << std::setiosflags (std::ios::left) << std::setw (8) << "FlowId" << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << "Size" << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << "Sent" << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << "Recv" << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << "FCT" << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << "PfcDur" << spaces;
            return oss.str();
        }
        std::string to_string()
        {
            uint32_t n_blackspaces = 8;
            std::string spaces = "";
            for (uint32_t i = 0; i < n_blackspaces; i++)
            {
                spaces += " ";
            } 
            std::ostringstream oss;
            oss << std::setiosflags (std::ios::left) << std::setw (8) << flowId << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << 1.0*flowsize/1000 << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << 1.0*sendSizeInbyte/1000  << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << 1.0*receSizeInbyte/1000 << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << 1.0*(finishTime - installTime)/1000 << spaces;
            oss << std::setiosflags (std::ios::left) << std::setw (10) << 1.0*pfcDuration/1000 << spaces;
            return oss.str();
            // std::string str = "flowId=" + std::to_string(flowId) + ", ";
            // str += "sentData=" + std::to_string(sendSizeInbyte) + ", ";
            // str += "receData=" + std::to_string(receSizeInbyte) + ", ";
            // // str += "sentAck=" + std::to_string(sendAckInbyte) + ", ";
            // // str += "receAck=" + std::to_string(receAckInbyte) + ", ";
            // str += "sentPkt=" + std::to_string(sendPacketNum) + ", ";
            // str += "recePkt=" + std::to_string(recePacketNum) + ", ";
            // str += "flowsize=" + std::to_string(flowsize) + ", ";
            // str += "installTime=" + std::to_string(installTime) + ", ";
            // str += "lastVistTime=" + std::to_string(lastVistTime) + ", ";
            // str += "windowSize=" + std::to_string(windowSize) + ", ";
            // str += "hdrInfo=" + hdrInfo + ", ";
            // str += "isFinished=" + std::to_string(isFinished) + ", ";
            // str += "pauseReason=" + pauseReason + ", ";
            // // str += "pauseCount=" + std::to_string(pauseCount) + ", ";
            // // str += "initialRate=" + std::to_string(initialRate) + ", ";
            // str += "nxtAvailTime=" + std::to_string(nxtAvailTime) + ", ";
            // str += "lastSelectedTime=" + std::to_string(lastSelectedTime) + ", ";
            // str += "snd_una=" + std::to_string(snd_una) + ", ";
            // str += "snd_nxt=" + std::to_string(snd_nxt);
            // return str;
        }
    };

    struct PlbEntry
    {
        uint32_t congested_rounds;
        uint32_t pkts_in_flight;
        uint32_t randomNum = 0;
        Time pause_until = Seconds(0);
    };
    struct PlbRecordEntry
    {
        uint32_t congested_rounds;
        uint32_t pkts_in_flight;
        uint32_t randomNum = 1;
        uint32_t pause_untilInSec;
        std::string flowID;
    };
    struct letflowSaveEntry
    {
        uint32_t lastPort = 0;
        uint32_t currPort = 0;
        uint64_t activeTime = 0;
        uint64_t timeGap = 0;
    };
    class PlbRehashTag : public Tag
    {
    public:
        PlbRehashTag();

        void SetRandomNum(uint32_t num);
        uint32_t GetRandomNum();
        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

    private:
        uint32_t randomNum;
    };
}
#endif
