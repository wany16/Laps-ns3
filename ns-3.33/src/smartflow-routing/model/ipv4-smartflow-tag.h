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
#include "ns3/userdefinedfunction.h"
// #include "ns3/smartFlow-goldenStru.h"


#define PROBE_PKT_DIRECTION_FOR_FORWARD 0
#define PROBE_PKT_DIRECTION_FOR_BACK 1
#define PROBE_PKT_DIRECTION_FOR_UNFILLED 2

namespace ns3 {

class Ipv4SmartFlowProbeTag : public Tag
{
public:
    Ipv4SmartFlowProbeTag ();
    virtual ~Ipv4SmartFlowProbeTag ();

    void SetDirection (uint8_t direction);
    uint8_t GetDirection (void) const;
    void SetPathId (uint32_t pid);
    uint32_t GetPathId (void) const;
    void Initialize(uint32_t probePathId);
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;
    void get_path_info(uint32_t &pathId, uint32_t &latency);

private:
    uint8_t m_direction; // 0: src -> dst, 1: dst -> src
    uint32_t m_pathId;
    Time m_generatedTime;
};


class Ipv4SmartFlowPathTag : public Tag {
public:
    Ipv4SmartFlowPathTag ();
    virtual ~Ipv4SmartFlowPathTag ();

    void set_hop_idx (uint32_t currPathIdx);
    uint32_t get_the_current_hop_index (void) const;

    void SetPathId (uint32_t pathId);
    uint32_t get_path_id (void) const;

    void SetDre(uint32_t t);
    uint32_t GetDre (void) const;


    void SetTimeStamp (Time &timestamp);
    Time GetTimeStamp (void) const;

    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

private:
    uint32_t m_pathId;
    uint32_t m_hopCnt;  // 当前路由pathVec Id
    uint32_t m_dre;
    Time m_timestamp;
};



class Ipv4SmartFlowCongaTag : public Tag {
public:
    Ipv4SmartFlowCongaTag ();
    virtual ~Ipv4SmartFlowCongaTag ();

    void SetPathId (uint32_t pathId);
    uint32_t get_path_id (void) const;

    void SetDre(uint32_t t);
    uint32_t GetDre (void) const;

    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

private:
    uint32_t m_pathId;
    uint32_t m_dre;
};




template <uint32_t IDX>
class Ipv4SmartFlowLatencyTag : public Tag
{
public:
    Ipv4SmartFlowLatencyTag ();
    virtual ~Ipv4SmartFlowLatencyTag ();

    void set_data_by_pit_entry (PathData * pitEntry);
    LatencyData GetLatencyData (void) const;

    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

private:
    uint32_t m_pathId;
    Time m_generatedTime;  // 当前路由pathVec Id
    uint32_t m_latency;
};

template <uint32_t IDX>
Ipv4SmartFlowLatencyTag<IDX>::Ipv4SmartFlowLatencyTag () {
}
template <uint32_t IDX>
Ipv4SmartFlowLatencyTag<IDX>::~Ipv4SmartFlowLatencyTag () {
}

template <uint32_t IDX>
void Ipv4SmartFlowLatencyTag<IDX>::set_data_by_pit_entry (PathData * pitEntry) {
    m_pathId = pitEntry->pid;
    m_generatedTime = pitEntry->tsGeneration;  // 当前路由pathVec Id
    m_latency = pitEntry->latency;
    return;
}

template <uint32_t IDX>
LatencyData Ipv4SmartFlowLatencyTag<IDX>::GetLatencyData () const {
    LatencyData data;
    data.latencyInfo.first = m_pathId;
    data.tsGeneration = m_generatedTime;  // 当前路由pathVec Id
    data.latencyInfo.second = m_latency;
    return data;
}

template <uint32_t IDX>
TypeId Ipv4SmartFlowLatencyTag<IDX>::GetTypeId (void) {
    std::ostringstream oss;
    oss << "ns3::Ipv4SmartFlowLatencyTag<"<<IDX<<">";
    static TypeId tid = TypeId (oss.str ().c_str ())
      .SetParent<Tag> ()
      .SetGroupName ("Internet")
      .AddConstructor<Ipv4SmartFlowLatencyTag<IDX> > ();
    return tid;
}

template <uint32_t IDX>
TypeId Ipv4SmartFlowLatencyTag<IDX>::GetInstanceTypeId (void) const {
    return GetTypeId ();
}

template <uint32_t IDX>
uint32_t Ipv4SmartFlowLatencyTag<IDX>::GetSerializedSize (void) const { 
    return sizeof(uint32_t) + sizeof(double) + sizeof(uint32_t);
}

template <uint32_t IDX>
void Ipv4SmartFlowLatencyTag<IDX>::Serialize (TagBuffer i) const {
    i.WriteU32 (m_pathId);
    i.WriteDouble (m_generatedTime.GetNanoSeconds());
    i.WriteU32 (m_latency);
    return;  
}

template <uint32_t IDX>
void Ipv4SmartFlowLatencyTag<IDX>::Deserialize (TagBuffer i) {
    m_pathId = i.ReadU32 ();
    m_generatedTime = Time::FromDouble (i.ReadDouble (), Time::NS);
    m_latency = i.ReadU32 ();
    return;
}

template <uint32_t IDX>
void Ipv4SmartFlowLatencyTag<IDX>::Print (std::ostream &os) const {
    os << "LATENCY TAG <"<< IDX <<"> INFO : ";
    os << "<pathid=" << m_pathId <<", ";
    os << "latency=" << m_latency <<", ";
    os << "tsGeneration=" << m_generatedTime;
    os << ">" <<std::endl;
    return;
}









} // namespace ns3

#endif /* IPV4_SMARTFLOW_TAG_H */
