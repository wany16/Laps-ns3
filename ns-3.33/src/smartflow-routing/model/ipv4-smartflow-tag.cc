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

#include <stdint.h>
#include "ipv4-smartflow-tag.h"
#include "ns3/log.h"
#include "ns3/tag-buffer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Ipv4SmartFlowTag");

Ipv4SmartFlowProbeTag::Ipv4SmartFlowProbeTag () : m_direction(PROBE_PKT_DIRECTION_FOR_UNFILLED) {
    NS_LOG_FUNCTION (this);
}

Ipv4SmartFlowProbeTag::~Ipv4SmartFlowProbeTag () {
    NS_LOG_FUNCTION (this);
}

void Ipv4SmartFlowProbeTag::Initialize(uint32_t probePathId){
    m_direction = PROBE_PKT_DIRECTION_FOR_FORWARD;
    m_pathId = probePathId;
    m_generatedTime = Simulator::Now();
}

void Ipv4SmartFlowProbeTag::get_path_info(uint32_t &pathId, uint32_t &latency){
    latency =  Simulator::Now().GetNanoSeconds() - m_generatedTime.GetNanoSeconds();
    pathId = m_pathId;
    return ;
}

void Ipv4SmartFlowProbeTag::SetDirection (uint8_t direction) {
    m_direction = direction;
    return;
}

uint8_t Ipv4SmartFlowProbeTag::GetDirection (void) const {
    return m_direction;
}

void Ipv4SmartFlowProbeTag::SetPathId (uint32_t pid) {
    m_pathId = pid;
    return;
}

uint32_t Ipv4SmartFlowProbeTag::GetPathId (void) const {
    return m_pathId;
}

TypeId Ipv4SmartFlowProbeTag::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::Ipv4SmartFlowProbeTag")
      .SetParent<Tag> ()
      .SetGroupName ("Internet")
      .AddConstructor<Ipv4SmartFlowProbeTag> ();
    return tid;
}

TypeId Ipv4SmartFlowProbeTag::GetInstanceTypeId (void) const {
    return GetTypeId ();
}

uint32_t Ipv4SmartFlowProbeTag::GetSerializedSize (void) const { 
    NS_LOG_FUNCTION (this);
    return sizeof(uint8_t) + sizeof(uint32_t) + sizeof(double);
}

void Ipv4SmartFlowProbeTag::Serialize (TagBuffer i) const {
    NS_LOG_FUNCTION (this << &i);
    i.WriteU8 (m_direction);
    i.WriteU32 (m_pathId);
    i.WriteDouble (m_generatedTime.GetNanoSeconds());
    return;  
}

void Ipv4SmartFlowProbeTag::Deserialize (TagBuffer i) {
    NS_LOG_FUNCTION (this<< &i);
    m_direction = i.ReadU8 ();
    m_pathId = i.ReadU32 ();
    m_generatedTime = Time::FromDouble (i.ReadDouble (), Time::NS);
    return;
}

void Ipv4SmartFlowProbeTag::Print (std::ostream &os) const
{
    NS_LOG_FUNCTION (this << &os);
    os << "SMART FLOW probe tag INFO : direction " << m_direction;
    os << ", m_latencyInfo pathid: " << m_pathId;
    os << ", m_latencyInfo generated time: " << m_generatedTime;
    os << "] ";
    return;
}





Ipv4SmartFlowPathTag::Ipv4SmartFlowPathTag ()
  : m_pathId (UINT32_MAX),
    m_hopCnt (UINT32_MAX),
    m_timestamp(Time (0)) {
    NS_LOG_FUNCTION (this);
}
Ipv4SmartFlowPathTag::~Ipv4SmartFlowPathTag () {
    NS_LOG_FUNCTION (this);
}
void Ipv4SmartFlowPathTag::set_hop_idx (uint32_t currPathIdx) {
    m_hopCnt = currPathIdx;
    return;
}
uint32_t Ipv4SmartFlowPathTag::get_the_current_hop_index (void) const{
    return m_hopCnt;
}
void Ipv4SmartFlowPathTag::SetPathId (uint32_t pathId) {
    m_pathId = pathId;
    return;
}
uint32_t Ipv4SmartFlowPathTag::get_path_id (void) const {
    return m_pathId;
}

void Ipv4SmartFlowPathTag::SetDre (uint32_t t) {
    m_dre = t;
    return;
}
uint32_t Ipv4SmartFlowPathTag::GetDre (void) const {
    return m_dre;
}




void Ipv4SmartFlowPathTag::SetTimeStamp (Time &timestamp) {
    m_timestamp = timestamp;
    return;
}
Time Ipv4SmartFlowPathTag::GetTimeStamp (void) const {
    return m_timestamp;
}
TypeId Ipv4SmartFlowPathTag::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::Ipv4SmartFlowPathTag")
      .SetParent<Tag> ()
      .SetGroupName ("Internet")
      .AddConstructor<Ipv4SmartFlowPathTag> ();
    return tid;
}
TypeId Ipv4SmartFlowPathTag::GetInstanceTypeId (void) const {
    return GetTypeId ();
}
uint32_t Ipv4SmartFlowPathTag::GetSerializedSize (void) const { 
    NS_LOG_FUNCTION (this);
    return sizeof (uint32_t) + sizeof (uint32_t) + sizeof (uint32_t) + sizeof(double);
}
void Ipv4SmartFlowPathTag::Serialize (TagBuffer i) const {
    NS_LOG_FUNCTION (this << &i);
    i.WriteU32 (m_pathId);
    i.WriteU32 (m_hopCnt);
    i.WriteU32 (m_dre);
    i.WriteDouble (m_timestamp.GetNanoSeconds ());
    return;  
}
void Ipv4SmartFlowPathTag::Deserialize (TagBuffer i) {
    NS_LOG_FUNCTION (this<< &i);
    m_pathId = i.ReadU32 ();
    m_hopCnt = i.ReadU32 ();
    m_dre = i.ReadU32 ();
    m_timestamp = Time::FromDouble (i.ReadDouble (), Time::NS);
    return;
}
void Ipv4SmartFlowPathTag::Print (std::ostream &os) const {
    NS_LOG_FUNCTION (this << &os);
    os << "PATH TAG: ";
    os << "Pid=" << m_pathId << ", ";
    os << "CurHopIdx=" << m_hopCnt << ", ";
    os << "Dre=" << m_dre << ", ";
    os << "StampTime=" << m_timestamp.GetNanoSeconds();
    os <<std::endl;
    return;
}









Ipv4SmartFlowCongaTag::Ipv4SmartFlowCongaTag ()
  : m_pathId (UINT32_MAX),
    m_dre(0) {
    NS_LOG_FUNCTION (this);
}
Ipv4SmartFlowCongaTag::~Ipv4SmartFlowCongaTag () {
    NS_LOG_FUNCTION (this);
}

void Ipv4SmartFlowCongaTag::SetPathId (uint32_t pathId) {
    m_pathId = pathId;
    return;
}
uint32_t Ipv4SmartFlowCongaTag::get_path_id (void) const {
    return m_pathId;
}

void Ipv4SmartFlowCongaTag::SetDre (uint32_t t) {
    m_dre = t;
    return;
}
uint32_t Ipv4SmartFlowCongaTag::GetDre (void) const {
    return m_dre;
}

TypeId Ipv4SmartFlowCongaTag::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::Ipv4SmartFlowCongaTag")
      .SetParent<Tag> ()
      .SetGroupName ("Internet")
      .AddConstructor<Ipv4SmartFlowCongaTag> ();
    return tid;
}
TypeId Ipv4SmartFlowCongaTag::GetInstanceTypeId (void) const {
    return GetTypeId ();
}
uint32_t Ipv4SmartFlowCongaTag::GetSerializedSize (void) const { 
    NS_LOG_FUNCTION (this);
    return sizeof (uint32_t) + sizeof (uint32_t);
}
void Ipv4SmartFlowCongaTag::Serialize (TagBuffer i) const {
    NS_LOG_FUNCTION (this << &i);
    i.WriteU32 (m_pathId);
    i.WriteU32 (m_dre);
    return;  
}
void Ipv4SmartFlowCongaTag::Deserialize (TagBuffer i) {
    NS_LOG_FUNCTION (this<< &i);
    m_pathId = i.ReadU32 ();
    m_dre = i.ReadU32 ();
    return;
}
void Ipv4SmartFlowCongaTag::Print (std::ostream &os) const {
    NS_LOG_FUNCTION (this << &os);
    os << "Conga TAG: ";
    os << "Pid=" << m_pathId << ", ";
    os << "Dre=" << m_dre << ", ";
    os <<std::endl;
    return;
}



} // namespace ns3

