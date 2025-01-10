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
size_t GetHashValue(std::vector<T> & key)
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
std::string ListToString(const std::list<std::pair<T1, T2>>& d)
{
    std::ostringstream oss;
    for (const auto& item : d) {
        oss << "(" << item.first << ", " << item.second << "), ";
    }
    std::string result = oss.str();
    if (!result.empty()) {
        result.pop_back(); // 移除最后一个空格
        result.pop_back(); // 移除最后一个逗号
    }
    else
    {
        result = "()";
    }
    return result;
}


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
        std::map<HostId2PathSeleKey, pdt_entry_t> m_pathDecTbl;
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

}
#endif
