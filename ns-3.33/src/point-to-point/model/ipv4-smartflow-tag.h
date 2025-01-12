/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hajime Tazaki
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: xudong <xudong@cmss.chinamobile.com>
 */

#ifndef IPV4_SMARTFLOW_TAG_H
#define IPV4_SMARTFLOW_TAG_H

#include <vector>
#include <utility>
#include <stdint.h>
#include <iomanip>

#include "ns3/tag.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
// #include "ns3/userdefinedfunction.h"
//   #include "ns3/smartFlow-goldenStru.h"

#define PROBE_PKT_DIRECTION_FOR_FORWARD 0
#define PROBE_PKT_DIRECTION_FOR_BACK 1
#define PROBE_PKT_DIRECTION_FOR_UNFILLED 2

namespace ns3
{
    template <typename T>
    std::string change2string(T src)
    {
        std::stringstream ss;
        uint32_t default_precision_for_float2string = 4;
        ss << std::fixed << std::setprecision(default_precision_for_float2string);
        ss << src;
        std::string str = ss.str();
        return str;
    }

    template <typename T>
    std::string vectorTostring(std::vector<T> src)
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
                std::string curStr = change2string<T>(src[i]);
                str = str + curStr + ", ";
            }
            std::string curStr = change2string<T>(src[vecSize - 1]);
            str = str + curStr + "]";
            return str;
        }
    }

    struct PathData
    {
        uint32_t pid=UINT32_MAX;
        uint32_t priority;
        std::vector<uint32_t> portSequence;
        std::vector<uint32_t> nodeIdSequence;
        uint32_t latency=0;
        uint32_t theoreticalSmallestLatencyInNs=0;
        uint32_t pathDre = UINT32_MAX;
        Time tsGeneration=Seconds(0);
        Time tsProbeLastSend=Seconds(0);
        Time tsLatencyLastSend=Seconds(0);
        Time  updateTime;
        Time _invalidTime = Seconds(0); // for conweave route
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
            std::cout << "NodeIDs=" << vectorTostring<uint32_t>(nodeIdSequence) << ", ";
            std::cout << "PortIDs=" << vectorTostring<uint32_t>(portSequence) << " ";
            std::cout << std::endl;
        }
        std::string toString()
        {
            std::string str = "PathId=" + change2string<uint32_t>(pid) + ", ";
            str = str + "Priority=" + change2string<uint32_t>(priority) + ", ";
            str = str + "Latency=" + change2string<uint64_t>(latency) + "ns, ";
            str = str + "ThLatency=" + change2string<uint64_t>(theoreticalSmallestLatencyInNs) + "ns, ";
            str = str + "GenTime=" + change2string<uint64_t>(tsGeneration.GetNanoSeconds()) + "ns, ";
            str = str + "PrbTime=" + change2string<uint64_t>(tsProbeLastSend.GetNanoSeconds()) + "ns, ";
            str = str + "SntTime=" + change2string<uint64_t>(tsLatencyLastSend.GetNanoSeconds()) + "ns, ";
            str = str + "NodeIDs=" + vectorTostring<uint32_t>(nodeIdSequence) + ", ";
            str = str + "PortIDs=" + vectorTostring<uint32_t>(portSequence);
            return str;
        }
    };
    struct LatencyData
    {
        std::pair<uint32_t, uint32_t> latencyInfo;
        Time tsGeneration;
        std::string toString()
        {
            std::string str = "PathId=" + change2string<uint32_t>(latencyInfo.first) + ", ";
            str = str + "Latency=" + change2string<uint32_t>(latencyInfo.second) + "ns, ";
            str = str + "GenTime=" + change2string<uint64_t>(tsGeneration.GetNanoSeconds()) + "ns";
            return str;
        }
    };
    class Ipv4SmartFlowProbeTag : public Tag
    {
    public:
        Ipv4SmartFlowProbeTag();
        virtual ~Ipv4SmartFlowProbeTag();

        void SetDirection(uint8_t direction);
        uint8_t GetDirection(void) const;
        void SetPathId(uint32_t pid);
        uint32_t GetPathId(void) const;
        void Initialize(uint32_t probePathId);
        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;
        void get_path_info(uint32_t &pathId, uint32_t &latency);

    private:
        uint8_t m_direction; // 0: src -> dst, 1: dst -> src
        uint32_t m_pathId;
        Time m_generatedTime;
    };

    class AckPathTag : public Tag
    {
    public:
        AckPathTag();
        virtual ~AckPathTag();

        void SetPathId(uint32_t pid);
        void SetFlowId(uint32_t fid);
        uint32_t GetPathId(void) const;
        uint32_t GetFlowId(void) const;
        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

    private:
        uint32_t m_pathId;
        uint32_t m_flowId;
    };

    class Ipv4SmartFlowPathTag : public Tag
    {
    public:
        Ipv4SmartFlowPathTag();
        virtual ~Ipv4SmartFlowPathTag();

        void set_hop_idx(uint32_t currPathIdx);
        uint32_t get_the_current_hop_index(void) const;

        void SetPathId(uint32_t pathId);
        uint32_t get_path_id(void) const;

        void SetDre(uint32_t t);
        uint32_t GetDre(void) const;

        void SetTimeStamp(Time &timestamp);
        Time GetTimeStamp(void) const;

        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

    private:
        uint32_t m_pathId;
        uint32_t m_hopCnt; // 当前路由pathVec Id
        uint32_t m_dre;
        Time m_timestamp;
    };

    class Ipv4SmartFlowCongaTag : public Tag
    {
    public:
        Ipv4SmartFlowCongaTag();
        virtual ~Ipv4SmartFlowCongaTag();

        void SetPathId(uint32_t pathId);
        uint32_t get_path_id(void) const;

        void SetDre(uint32_t t);
        uint32_t GetDre(void) const;

        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

    private:
        uint32_t m_pathId;
        uint32_t m_dre;
    };

    template <uint32_t IDX>
    class Ipv4SmartFlowLatencyTag : public Tag
    {
    public:
        Ipv4SmartFlowLatencyTag();
        virtual ~Ipv4SmartFlowLatencyTag();

        void set_data_by_pit_entry(PathData *pitEntry);
        LatencyData GetLatencyData(void) const;

        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

    private:
        uint32_t m_pathId;
        Time m_generatedTime; // 当前路由pathVec Id
        uint32_t m_latency;
    };

    template <uint32_t IDX>
    Ipv4SmartFlowLatencyTag<IDX>::Ipv4SmartFlowLatencyTag()
    {
    }
    template <uint32_t IDX>
    Ipv4SmartFlowLatencyTag<IDX>::~Ipv4SmartFlowLatencyTag()
    {
    }

    template <uint32_t IDX>
    void Ipv4SmartFlowLatencyTag<IDX>::set_data_by_pit_entry(PathData *pitEntry)
    {
        m_pathId = pitEntry->pid;
        m_generatedTime = pitEntry->tsGeneration; // 当前路由pathVec Id
        m_latency = pitEntry->latency;
        return;
    }

    template <uint32_t IDX>
    LatencyData Ipv4SmartFlowLatencyTag<IDX>::GetLatencyData() const
    {
        LatencyData data;
        data.latencyInfo.first = m_pathId;
        data.tsGeneration = m_generatedTime; // 当前路由pathVec Id
        data.latencyInfo.second = m_latency;
        return data;
    }

    template <uint32_t IDX>
    TypeId Ipv4SmartFlowLatencyTag<IDX>::GetTypeId(void)
    {
        std::ostringstream oss;
        oss << "ns3::Ipv4SmartFlowLatencyTag<" << IDX << ">";
        static TypeId tid = TypeId(oss.str().c_str())
                                .SetParent<Tag>()
                                .SetGroupName("Internet")
                                .AddConstructor<Ipv4SmartFlowLatencyTag<IDX>>();
        return tid;
    }

    template <uint32_t IDX>
    TypeId Ipv4SmartFlowLatencyTag<IDX>::GetInstanceTypeId(void) const
    {
        return GetTypeId();
    }

    template <uint32_t IDX>
    uint32_t Ipv4SmartFlowLatencyTag<IDX>::GetSerializedSize(void) const
    {
        return sizeof(uint32_t) + sizeof(double) + sizeof(uint32_t);
    }

    template <uint32_t IDX>
    void Ipv4SmartFlowLatencyTag<IDX>::Serialize(TagBuffer i) const
    {
        i.WriteU32(m_pathId);
        i.WriteDouble(m_generatedTime.GetNanoSeconds());
        i.WriteU32(m_latency);
        return;
    }

    template <uint32_t IDX>
    void Ipv4SmartFlowLatencyTag<IDX>::Deserialize(TagBuffer i)
    {
        m_pathId = i.ReadU32();
        m_generatedTime = Time::FromDouble(i.ReadDouble(), Time::NS);
        m_latency = i.ReadU32();
        return;
    }

    template <uint32_t IDX>
    void Ipv4SmartFlowLatencyTag<IDX>::Print(std::ostream &os) const
    {
        os << "LATENCY TAG <" << IDX << "> INFO : ";
        os << "<pathid=" << m_pathId << ", ";
        os << "latency=" << m_latency << ", ";
        os << "tsGeneration=" << m_generatedTime;
        os << ">" << std::endl;
        return;
    }

} // namespace ns3

#endif /* IPV4_SMARTFLOW_TAG_H */
