/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
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
* Author: Yuliang Li <yuliangli@g.harvard.com>
*/

#define __STDC_LIMIT_MACROS 1
#include <stdint.h>
#include <stdio.h>
#include "ns3/qbb-net-device.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/object-vector.h"
#include "ns3/pause-header.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/assert.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "ns3/simulator.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/qbb-channel.h"
#include "ns3/random-variable-stream.h"
#include "ns3/flow-id-tag.h"
#include "ns3/qbb-header.h"
#include "ns3/error-model.h"
#include "ns3/cn-header.h"
#include "ns3/ppp-header.h"
#include "ns3/udp-header.h"
#include "ns3/seq-ts-header.h"
#include "ns3/pointer.h"
#include "ns3/custom-header.h"
#include "ns3/switch-node.h"
#include <iostream>

#define MAP_KEY_EXISTS(map, key) (((map).find(key) != (map).end()))
#define INDEX_HIGH

NS_LOG_COMPONENT_DEFINE("QbbNetDevice");

namespace ns3 {
	
	uint32_t RdmaEgressQueue::ack_q_idx = 0;
	uint32_t RdmaEgressQueue::mtuInByte = 1400;
  std::unordered_map<int32_t, Time> RdmaEgressQueue::cumulative_pause_time;
	bool RdmaEgressQueue::isAckHighPriority = true;


	// RdmaEgressQueue
	TypeId RdmaEgressQueue::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::RdmaEgressQueue")
			.SetParent<Object> ()
			.AddTraceSource ("RdmaEnqueue", "Enqueue a packet in the RdmaEgressQueue.",
					MakeTraceSourceAccessor (&RdmaEgressQueue::m_traceRdmaEnqueue),
					"ns3::TracedCallback")
			.AddTraceSource ("RdmaDequeue", "Dequeue a packet in the RdmaEgressQueue.",
					MakeTraceSourceAccessor (&RdmaEgressQueue::m_traceRdmaDequeue),
					"ns3::TracedCallback")
			;
		return tid;
	}

	RdmaEgressQueue::RdmaEgressQueue(){
		m_rrlast = 0;
		m_qlast = 0;
		m_ackQ = CreateObject<DropTailQueue<Packet> >();
		// ziven: TODO fix this, 'MaxBytes' comes from ns-3.18
		// m_ackQ->SetAttribute("MaxBytes", UintegerValue(0xffffffff)); // queue limit is on a higher level, not here
	}

	Ptr<Packet> RdmaEgressQueue::DequeueQindex(int qIndex){
		if (qIndex == QINDEX_OF_ACK_PACKET_IN_SERVER){ // high prio
			Ptr<Packet> p = m_ackQ->Dequeue();
			m_qlast = QINDEX_OF_ACK_PACKET_IN_SERVER;
			m_traceRdmaDequeue(p, 0);
			return p;
		}
		if (qIndex >= 0){ // qp
			Ptr<Packet> p = m_rdmaGetNxtPkt(m_qpGrp->Get(qIndex));
			m_rrlast = qIndex;
			m_qlast = qIndex;
			m_traceRdmaDequeue(p, m_qpGrp->Get(qIndex)->m_pg);
			return p;
		}
		return 0;
	}
	int RdmaEgressQueue::GetNextQindex(bool paused[]){ // only for host
		// bool found = false;
		uint32_t qIndex;
		if (!paused[ack_q_idx] && m_ackQ->GetNPackets() > 0) // 0 : higher priority sent first, 3 : same priority, stored seperately but still sent first 
			return QINDEX_OF_ACK_PACKET_IN_SERVER;

		// no pkt in highest priority queue, do rr for each qp/flow
		uint32_t fcount = m_qpGrp->GetN();
		for (qIndex = 1; qIndex <= fcount; qIndex++){
			uint32_t idx = (qIndex + m_rrlast) % fcount;
			Ptr<RdmaQueuePair> qp = m_qpGrp->Get(idx);
			if (qp->IsFinishedConst()) m_qpGrp->SetQpFinished(idx);
      if (m_qpGrp->IsQpFinished(idx)) continue;
			bool isPfcAllowed = !paused[qp->m_pg];
			bool isWinAllowed = !qp->IsWinBound();
			bool isIrnAllowed = qp->CanIrnTransmit(mtuInByte);
			bool isDataLeft = qp->GetBytesLeft() > 0 ? true : false;
			bool isTimeAvail = qp->m_nextAvail.GetTimeStep() > Simulator::Now().GetTimeStep() ? false : true;
			int32_t flowid = qp->m_flow_id;

			if (!isPfcAllowed && isDataLeft && isWinAllowed && isIrnAllowed) {
					if (!isTimeAvail) { // not available now
					} else {// blocked by PFC
							if (!MAP_KEY_EXISTS(m_startPauseTime, flowid)) m_startPauseTime[flowid] = Simulator::Now();
					}
			}else if (isPfcAllowed && isDataLeft && isWinAllowed && isIrnAllowed) {
            if (!isTimeAvail)  continue; // not available now
						if (MAP_KEY_EXISTS(m_startPauseTime, flowid)) {             // Check if the flow has been blocked by PFC
								Time tdiff = Simulator::Now() - m_startPauseTime[flowid];
								if (!MAP_KEY_EXISTS(m_startPauseTime, flowid)) cumulative_pause_time[flowid] = Seconds(0);
								cumulative_pause_time[flowid] += tdiff;
								m_startPauseTime.erase(flowid);
						}
            return idx;
      }
		}
		return QINDEX_OF_NONE_PACKET_IN_SERVER;
	}

	int RdmaEgressQueue::GetLastQueue(){
		return m_qlast;
	}

	uint32_t RdmaEgressQueue::GetNBytes(uint32_t qIndex){
		NS_ASSERT_MSG(qIndex < m_qpGrp->GetN(), "RdmaEgressQueue::GetNBytes: qIndex >= m_qpGrp->GetN()");
		return m_qpGrp->Get(qIndex)->GetBytesLeft();
	}

	uint32_t RdmaEgressQueue::GetFlowCount(void){	return m_qpGrp->GetN();	}
	Ptr<RdmaQueuePair> RdmaEgressQueue::GetQp(uint32_t i){ return m_qpGrp->Get(i);}
 
	void RdmaEgressQueue::RecoverQueue(uint32_t i){
		NS_ASSERT_MSG(i < m_qpGrp->GetN(), "RdmaEgressQueue::RecoverQueue: qIndex >= m_qpGrp->GetN()");
		m_qpGrp->Get(i)->snd_nxt = m_qpGrp->Get(i)->snd_una;
	}

	void RdmaEgressQueue::EnqueueHighPrioQ(Ptr<Packet> p){
		m_traceRdmaEnqueue(p, 0);
		m_ackQ->Enqueue(p);
	}

	void RdmaEgressQueue::CleanHighPrio(TracedCallback<Ptr<const Packet>, uint32_t> dropCb){
		while (m_ackQ->GetNPackets() > 0){
			Ptr<Packet> p = m_ackQ->Dequeue();
			dropCb(p, 0);
		}
	}

	/******************
	 * QbbNetDevice
	 *****************/
	NS_OBJECT_ENSURE_REGISTERED(QbbNetDevice);

	TypeId
		QbbNetDevice::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::QbbNetDevice")
								.SetParent<PointToPointNetDevice>()
								.AddConstructor<QbbNetDevice>()
								.AddAttribute("QbbEnabled",
											  "Enable the generation of PAUSE packet.",
											  BooleanValue(true),
											  MakeBooleanAccessor(&QbbNetDevice::m_qbbEnabled),
											  MakeBooleanChecker())
								.AddAttribute("QcnEnabled",
											  "Enable the generation of PAUSE packet.",
											  BooleanValue(false),
											  MakeBooleanAccessor(&QbbNetDevice::m_qcnEnabled),
											  MakeBooleanChecker())
								.AddAttribute("DynamicThreshold",
											  "Enable dynamic threshold.",
											  BooleanValue(false),
											  MakeBooleanAccessor(&QbbNetDevice::m_dynamicth),
											  MakeBooleanChecker())
								.AddAttribute("PauseTime",
											  "Number of microseconds to pause upon congestion",
											  UintegerValue(5),
											  MakeUintegerAccessor(&QbbNetDevice::m_pausetime),
											  MakeUintegerChecker<uint32_t>())
								.AddAttribute("TxBeQueue",
											  "A queue to use as the transmit queue in the device.",
											  PointerValue(),
											  MakePointerAccessor(&QbbNetDevice::m_queue),
											  MakePointerChecker<Queue<Packet>>())
								.AddAttribute("RdmaEgressQueue",
											  "A queue to use as the transmit queue in the device.",
											  PointerValue(),
											  MakePointerAccessor(&QbbNetDevice::m_rdmaEQ),
											  MakePointerChecker<Object>())
								.AddTraceSource("QbbEnqueue", "Enqueue a packet in the QbbNetDevice.",
												MakeTraceSourceAccessor(&QbbNetDevice::m_traceEnqueue),
												"ns3::TracedCallback")
								.AddTraceSource("QbbDequeue", "Dequeue a packet in the QbbNetDevice.",
												MakeTraceSourceAccessor(&QbbNetDevice::m_traceDequeue),
												"ns3::TracedCallback")
								.AddTraceSource("QbbDrop", "Drop a packet in the QbbNetDevice.",
												MakeTraceSourceAccessor(&QbbNetDevice::m_traceDrop),
												"ns3::TracedCallback")
								.AddTraceSource("RdmaQpDequeue", "A qp dequeue a packet.",
												MakeTraceSourceAccessor(&QbbNetDevice::m_traceQpDequeue),
												"ns3::TracedCallback")
								.AddTraceSource("QbbPfc", "get a PFC packet. 0: resume, 1: pause",
												MakeTraceSourceAccessor(&QbbNetDevice::m_tracePfc),
												"ns3::TracedCallback")
								.AddAttribute("QbbE2ELb", "E2E Load balancing algorithm.",
											  EnumValue(LB_Solution::LB_NONE),
											  MakeEnumAccessor(&QbbNetDevice::m_lbSolution),
											  MakeEnumChecker(LB_Solution::LB_E2ELAPS, "e2elaps"));

		return tid;
	}

	QbbNetDevice::QbbNetDevice()
	{
		NS_LOG_FUNCTION(this);
		m_ecn_source = new std::vector<ECNAccount>;
		for (uint32_t i = 0; i < qCnt; i++){
			m_paused[i] = false;
		}

		m_rdmaEQ = CreateObject<RdmaEgressQueue>();
	}

	QbbNetDevice::~QbbNetDevice()
	{
		NS_LOG_FUNCTION(this);
	}

	void
		QbbNetDevice::DoDispose()
	{
		NS_LOG_FUNCTION(this);

		PointToPointNetDevice::DoDispose();
	}

   bool QbbNetDevice::IsPfcEnabled() const {
		return m_qbbEnabled;
	 }


	void
		QbbNetDevice::TransmitComplete(void)
	{
		NS_LOG_FUNCTION(this);
		NS_ASSERT_MSG(m_txMachineState == BUSY, "Must be BUSY if transmitting");
		m_txMachineState = READY;
		NS_ASSERT_MSG(m_currentPkt != 0, "QbbNetDevice::TransmitComplete(): m_currentPkt zero");
		m_phyTxEndTrace(m_currentPkt);
		m_currentPkt = 0;
		DequeueAndTransmit();
	}
	bool
	QbbNetDevice::LbPacketTransmitStart(Ptr<E2ESrcOutPackets> &srcOutEntryPtr, bool Isack)
	{

		//
		// This function is called to start the process of transmitting a packet.
		// We need to tell the channel that we've started wiggling the wire and
		// schedule an event that will be executed when the transmission is complete.
		//

		Ptr<Packet> p, probe;
		p = srcOutEntryPtr->dataPacket;
		NS_LOG_FUNCTION(this << p);
		NS_LOG_LOGIC("UID is " << p->GetUid() << ")");
		NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
		m_txMachineState = BUSY;
		m_currentPkt = p;
		m_phyTxBeginTrace(m_currentPkt);

		Time txTime1 = Seconds(m_bps.CalculateTxTime(p->GetSize()));
		Time txCompleteTime = txTime1 + m_tInterframeGap;
		Time allInterframeGap = m_tInterframeGap;
		uint32_t allPktSize = p->GetSize();
		if (srcOutEntryPtr->Isprobe) // 遗留bug,探测报文不能正常发送；
		{
			probe = srcOutEntryPtr->probePacket;
			Time txTime2 = Seconds(m_bps.CalculateTxTime(probe->GetSize()));
			txCompleteTime += (txTime2 + m_tInterframeGap);
			bool result1 = m_channel->TransmitStart(probe, this, txTime2);
			if (result1 == false)
			{
				m_phyTxDropTrace(p);
			}
			NS_LOG_LOGIC("Probe packet transmitCompleteEvent in " << txTime2.GetSeconds() << "sec");
			allInterframeGap += m_tInterframeGap;
			allPktSize += probe->GetSize();
		}
		NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds() << "sec");
		Simulator::Schedule(txCompleteTime, &QbbNetDevice::TransmitComplete, this);

		bool result = m_channel->TransmitStart(p, this, txTime1);
		if (result == false)
		{
			m_phyTxDropTrace(p);
		}

		// int qIndex = m_rdmaEQ->GetNextQindex(m_paused);
		if (!srcOutEntryPtr->Isack)
		{
			m_rdmaLbPktSent(srcOutEntryPtr->lastQp, allPktSize, allInterframeGap);
		}

		return result;
	}

	bool QbbNetDevice::ApplyLoadBalancingSolution(uint32_t qIndex, Ptr<E2ESrcOutPackets> &srcOutEntryPtr)
	{
		bool Isack = false;
		Ptr<Packet> p;
		if (qIndex == QINDEX_OF_ACK_PACKET_IN_SERVER)
		{ // exist ack packets in the highest priority queue
			p = m_rdmaEQ->DequeueQindex(QINDEX_OF_ACK_PACKET_IN_SERVER);
			m_traceDequeue(p, 0);
			srcOutEntryPtr->Isack = true;
		}
		else
		{
			// no ack packet in the highest priority queue, so to process the qIndex-th queue
			Ptr<RdmaQueuePair> lastQp = m_rdmaEQ->GetQp(qIndex); // to dequeue a packet in a RR manner
			p = m_rdmaEQ->DequeueQindex(qIndex);
			m_traceQpDequeue(p, lastQp);
			// m_rdmaPktSent(lastQp, p, m_tInterframeGap);
			srcOutEntryPtr->lastQp = lastQp;
			srcOutEntryPtr->Isack = false;
		}
		// extract customheader
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
						CustomHeader::L4_Header);
		if (p->PeekHeader(ch) > 0)
		{
			NS_LOG_INFO("QbbNetDevice::ApplyLoadBalancingSolution Extracted CustomHeader:");
		}
		else
		{
			NS_LOG_WARN("QbbNetDevice::ApplyLoadBalancingSolution Failed to extract CustomHeader from the packet.");
		}

		Ptr<RdmaSmartFlowRouting> m_routing = m_rdmaGetE2ELapsLBouting();
		m_routing->RouteOutput(p, ch, srcOutEntryPtr);

		return Isack;
	}

	void
	QbbNetDevice::DequeueAndTransmit(void)
	{
		NS_LOG_FUNCTION(this);
		if (!m_linkUp) return; // if link is down, return
		if (m_txMachineState == BUSY) return;	// Quit if channel busy
		Ptr<Packet> p;
		NS_LOG_INFO("----------------MyNode:" << m_node->GetId());
		if (m_node->GetId() == 18)
		{
			NS_LOG_INFO("MyNode is 18" << " testcount:" << testcount);
		}
		if (m_node->GetNodeType() == NODE_TYPE_OF_SERVER)
		{													// for netdevice in host, has 8 virtual queues
			int qIndex = m_rdmaEQ->GetNextQindex(m_paused); // the index of the qp (NOT queue or virtual queue) to send next
			if (qIndex != QINDEX_OF_NONE_PACKET_IN_SERVER)
			{ // exist packet to send
				if (m_lbSolution == LB_Solution::LB_E2ELAPS)
				{
					std::cout << "----------------ServerNode:" << m_node->GetId() << " ,Qbbdevice application LB_E2ELAPS" << std::endl;
					NS_LOG_INFO("Qbbdevice application LB_E2ELAPS");
					// Ptr<E2ESrcOutPackets> srcOutEntryPtr = new E2ESrcOutPackets();
					Ptr<E2ESrcOutPackets> srcOutEntryPtr = Create<E2ESrcOutPackets>();
					NS_LOG_INFO("Qbbdevice111111 application LB_E2ELAPS");
					ApplyLoadBalancingSolution(qIndex, srcOutEntryPtr);
					LbPacketTransmitStart(srcOutEntryPtr, false);
				}
				else
				{

					if (qIndex == QINDEX_OF_ACK_PACKET_IN_SERVER)
					{																 // exist ack packets in the highest priority queue
						p = m_rdmaEQ->DequeueQindex(QINDEX_OF_ACK_PACKET_IN_SERVER); // get the actual packet to send
						m_traceDequeue(p, 0);										 // trace the current packet
						TransmitStart(p);											 // start the sending action
						return;
					}
					// no ack packet in the highest priority queue, so to process the qIndex-th queue
					Ptr<RdmaQueuePair> lastQp = m_rdmaEQ->GetQp(qIndex); // to dequeue a packet in a RR manner
					p = m_rdmaEQ->DequeueQindex(qIndex);
					// transmit
					m_traceQpDequeue(p, lastQp);
					TransmitStart(p);
					// update for the next avail time
					m_rdmaPktSent(lastQp, p, m_tInterframeGap);
				}
			}
			else
			{ // no packet to send
				// NS_LOG_INFO("1 PAUSE prohibits send at node " << m_node->GetId());
				Time t = Simulator::GetMaximumSimulationTime();
				bool valid = false;
				for (uint32_t i = 0; i < m_rdmaEQ->GetFlowCount(); i++)
				{
					Ptr<RdmaQueuePair> qp = m_rdmaEQ->GetQp(i);
					if (qp->GetBytesLeft() == 0)
						continue;
					t = Min(qp->m_nextAvail, t);
					valid = true;
				}
				if (valid && m_nextSend.IsExpired() && t < Simulator::GetMaximumSimulationTime() && t > Simulator::Now())
				{
					m_nextSend = Simulator::Schedule(t - Simulator::Now(), &QbbNetDevice::DequeueAndTransmit, this);
				}
			}
			return;
		}
		else
		{ // switch, doesn't care about qcn, just send

			testcount += 1;

			//  std::cout << "----------------Node:"<< m_node->GetId() << ", NIC:" << m_ifIndex << ", NBytesTotal:" << m_queue->GetNBytesTotal()<< "-----------------------" << std::endl;
			p = m_queue->DequeueRR(m_paused); // this is round-robin

			// if ((p == 0) &&  (m_node->GetId() == 0) && (m_queue->GetNBytesTotal() > 0) && (m_ifIndex == 8)) {
			// 		std::cout << "Error!!!!!!!!!!!!!!!!!!!!!!!!!" <<std::endl;
			// 		Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(m_node);
			// 		for (uint32_t j = 1; j < sw->GetNDevices(); j++){
			// 				std::cout << "NIC: "<< j << ", IngressLenInByte:[ ";
			// 				uint32_t s = 0;
			// 				for (uint32_t k = 0; k < qCnt; k++){
			// 						std::cout << sw->m_mmu->ingress_bytes[j][k] << " ";
			// 						s  = s + sw->m_mmu->ingress_bytes[j][k];
			// 				}
			// 				std::cout <<"] LenSumInByte: "<<s << ", egressLenInByte:[ ";
			// 				s = 0;
			// 				for (uint32_t k = 0; k < qCnt; k++){
			// 						std::cout << sw->m_mmu->egress_bytes[j][k] << " ";
			// 						s  = s + sw->m_mmu->egress_bytes[j][k];
			// 				}
			// 				std::cout <<"] LenSumInByte: "<<s << std::endl;

			// 		}
			// 	for (uint32_t i = 0; i < qCnt; i++){
			// 		if (m_paused[i] == false) {
			// 			std::cout << "Node:"<< m_node->GetId() << ", NIC:" << m_ifIndex << ", Queue:" << i <<", State:" << "NORMAL" << std::endl;
			// 		}else{
			// 			std::cout << "Node:"<< m_node->GetId() << ", NIC:" << m_ifIndex << ", Queue:" << i <<", State:" << "PAUSE" << std::endl;
			// 		}
			// 	}
			// }
			// 	std::cout << "**********************Finish*************************" << std::endl;

			if (p != 0)
			{
				NS_LOG_INFO("SwNode:" << m_node->GetId() << "UID is " << p->GetUid() << ")");
				m_snifferTrace(p);
				m_promiscSnifferTrace(p);
				Ipv4Header h;
				Ptr<Packet> packet = p->Copy();
				uint16_t protocol = 0;
				ProcessHeader(packet, protocol);
				packet->RemoveHeader(h);
				FlowIdTag t;
				uint32_t qIndex = m_queue->GetLastQueue();
				if (qIndex == 0)
				{ // this is a pause or cnp, send it immediately!
					DynamicCast<SwitchNode>(m_node)->SwitchNotifyDequeue(m_ifIndex, qIndex, p);
					p->RemovePacketTag(t);
				}
				else
				{
					DynamicCast<SwitchNode>(m_node)->SwitchNotifyDequeue(m_ifIndex, qIndex, p);
					p->RemovePacketTag(t);
				}
				m_traceDequeue(p, qIndex);
				TransmitStart(p);
				return;
			}
			else
			{ // No queue can deliver any packet
				// if (m_queue->GetNBytesTotal() != 0) {
				// 	for (uint32_t i = 0; i < qCnt; i++){
				// 		if (m_paused[i] == false) {
				// 			std::cout << "Node:"<< m_node->GetId() << ", NIC:" << m_ifIndex << ", Queue:" << i <<", State:" << "NORMAL" << std::endl;
				// 		}else{
				// 			std::cout << "Node:"<< m_node->GetId() << ", NIC:" << m_ifIndex << ", Queue:" << i <<", State:" << "PAUSE" << std::endl;
				// 		}
				// 	}
				// 	// std::cout << "***********************************************" << std::endl;
				// }

				// NS_LOG_INFO("2 PAUSE prohibits send at node " << m_node->GetId());
				if (m_node->GetNodeType() == 0 && m_qcnEnabled)
				{ // nothing to send, possibly due to qcn flow control, if so reschedule sending, !!!!!!!!!! Never run into the block below
					Time t = Simulator::GetMaximumSimulationTime();
					for (uint32_t i = 0; i < m_rdmaEQ->GetFlowCount(); i++)
					{
						Ptr<RdmaQueuePair> qp = m_rdmaEQ->GetQp(i);
						if (qp->GetBytesLeft() == 0)
							continue;
						t = Min(qp->m_nextAvail, t);
					}
					if (m_nextSend.IsExpired() && t < Simulator::GetMaximumSimulationTime() && t > Simulator::Now())
					{
						m_nextSend = Simulator::Schedule(t - Simulator::Now(), &QbbNetDevice::DequeueAndTransmit, this);
					}
				}
			}
			// std::cout << "**********************Finish*************************" << std::endl;
		}
		return;
	}

	void
		QbbNetDevice::Resume(unsigned qIndex)
	{
		// std::cout << "Node " << m_node->GetId() << " dev " << m_ifIndex << " queue " << qIndex << " is going to resume at " << Simulator::Now().GetSeconds() << std::endl;
		NS_LOG_FUNCTION(this << qIndex);
		// NS_ASSERT_MSG(m_paused[qIndex], "Must be PAUSEd");
		m_paused[qIndex] = false;
		// std::cout << "Node " << m_node->GetId() << " dev " << m_ifIndex << " queue " << qIndex << " resume at " << Simulator::Now().GetSeconds() << std::endl;
		// NS_LOG_INFO("Node " << m_node->GetId() << " dev " << m_ifIndex << " queue " << qIndex << " resumed at " << Simulator::Now().GetSeconds());
		DequeueAndTransmit();
	}

	void
		QbbNetDevice::Receive(Ptr<Packet> packet)
	{
		NS_LOG_FUNCTION(this << packet);
		if (!m_linkUp){
			m_traceDrop(packet, 0);
			return;
		}

		if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
		{
			// 
			// If we have an error model and it indicates that it is time to lose a
			// corrupted packet, don't forward this packet up, let it go.
			//
			m_phyRxDropTrace(packet);
			return;
		}

		m_macRxTrace(packet);
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		ch.getInt = 1; // parse INT header
		packet->PeekHeader(ch);
		if (ch.l3Prot == L3ProtType::PFC){ // PFC
			if (!m_qbbEnabled) return;
			unsigned qIndex = ch.pfc.qIndex;
			if (ch.pfc.time > 0){
				m_tracePfc(1);
				m_paused[qIndex] = true;
				// std::cout << "Node " << m_node->GetId() << " dev " << m_ifIndex << " queue " << qIndex << " pause at " << Simulator::Now().GetSeconds() << " last for " << ch.pfc.time << " us" << std::endl;
				Simulator::Cancel(m_resumeEvt[qIndex]);
				m_resumeEvt[qIndex] = Simulator::Schedule(MicroSeconds(ch.pfc.time), &QbbNetDevice::Resume, this, qIndex);
			}else{
				m_tracePfc(0);
        Simulator::Cancel(m_resumeEvt[qIndex]);
				Resume(qIndex);
			}
		}else { // non-PFC packets (data, ACK, NACK, CNP...)
			if (m_node->GetNodeType() == NODE_TYPE_OF_SWITCH){ // switch
				FlowIdTag flowIdTag;
				if (!packet->PeekPacketTag(flowIdTag))
				{
					// add flowid tag
					flowIdTag.SetFlowId(m_ifIndex);
					packet->AddPacketTag(flowIdTag);
				}
				else
				{
					FlowIdTag flowIdTag1;
					packet->PeekPacketTag(flowIdTag1);
					NS_LOG_INFO("FlowIdTag:" << flowIdTag1.GetFlowId() << " already exists on packet " << packet->GetUid() << " " << PARSE_FIVE_TUPLE(ch) << " nodeid: " << m_node->GetId());
					// pathTag.set_hop_idx(curHopIdx + 1);
					// packet->ReplacePacketTag(pathTag);
				}

				// packet->AddPacketTag(FlowIdTag(m_ifIndex));
				DynamicCast<SwitchNode>(m_node)->SwitchReceiveFromDevice(this, packet, ch);
			}else { // NIC
				// send to RdmaHw
				FlowIdTag flowIdTag1;
				Ipv4SmartFlowProbeTag probeTag;
				bool findProPacket = packet->PeekPacketTag(probeTag);
				if (findProPacket)
				{
					NS_LOG_INFO("Node " << m_node->GetId() << " Nic Receive ProbePacket");
				}
				if (!packet->PeekPacketTag(flowIdTag1) && findProPacket)
				{
					// add flowid tag
					// flowIdTag.SetFlowId(m_ifIndex);
					// packet->AddPacketTag(flowIdTag);
					NS_LOG_INFO("NOT FIND FLOWIDtag");
					// add flowid tag
					//flowIdTag1.SetFlowId(m_ifIndex);
					//packet->AddPacketTag(flowIdTag1);
				}
				else
				{
					packet->PeekPacketTag(flowIdTag1);
					NS_LOG_INFO("NIC FlowIdTag:" << flowIdTag1.GetFlowId() << " already exists on packet " << packet->GetUid() << " " << PARSE_FIVE_TUPLE(ch) << " nodeid: " << m_node->GetId());
					// pathTag.set_hop_idx(curHopIdx + 1);
					// packet->ReplacePacketTag(pathTag);
				}
				int ret = m_rdmaReceiveCb(packet, ch);
				(void)ret; // avoid warning
				// TODO we may based on the ret do something
			}
		}
		return;
	}

	bool QbbNetDevice::Send(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
	{
		NS_ASSERT_MSG(false, "QbbNetDevice::Send not implemented yet\n");
		return false;
	}

	bool QbbNetDevice::SwitchSend (uint32_t qIndex, Ptr<Packet> packet, CustomHeader &ch){
		m_macTxTrace(packet);
		m_traceEnqueue(packet, qIndex);
		m_queue->Enqueue(packet, qIndex);
		DequeueAndTransmit();
		return true;
	}

	uint32_t QbbNetDevice::SendPfc(uint32_t qIndex, uint32_t type){
		if (!m_qbbEnabled) return 0;
		Ptr<Packet> p = Create<Packet>(0);
		Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
		PauseHeader pauseh((type == PfcPktType::PAUSE ? m_pausetime : 0), m_queue->GetNBytes(qIndex), qIndex);
		if (type == PfcPktType::PAUSE)
		{
    	std::cout << "TimeInNs: " <<   Simulator::Now().GetNanoSeconds() << " Node: " << m_node->GetId() <<  " Nic: " << m_ifIndex  << " Qindex: "<<qIndex << " Send PAUSE" << std::endl;
		}else{
    	std::cout << "TimeInNs: " <<   Simulator::Now().GetNanoSeconds() << " Node: " << m_node->GetId() <<  " Nic: " << m_ifIndex  << " Qindex: "<<qIndex << " Send Resume" << std::endl;
		}
		

		p->AddHeader(pauseh);
		Ipv4Header ipv4h;  // Prepare IPv4 header
		ipv4h.SetProtocol(L3ProtType::PFC);
		ipv4h.SetSource(m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal());
		ipv4h.SetDestination(Ipv4Address("255.255.255.255"));
		ipv4h.SetPayloadSize(p->GetSize());
		ipv4h.SetTtl(1);
		ipv4h.SetIdentification(x->GetValue(0, 65536));
		p->AddHeader(ipv4h);
		AddHeader(p, 0x800);
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		p->PeekHeader(ch);
		SwitchSend(0, p, ch);
		return (type == PfcPktType::PAUSE ? m_pausetime : 0);
	}

	bool
		QbbNetDevice::Attach(Ptr<QbbChannel> ch)
	{
		NS_LOG_FUNCTION(this << &ch);
		m_channel = ch;
		m_channel->Attach(this);
		NotifyLinkUp();
		return true;
	}

	bool
		QbbNetDevice::TransmitStart(Ptr<Packet> p)
	{
		NS_LOG_FUNCTION(this << p);
		NS_LOG_LOGIC("UID is " << p->GetUid() << ")");
		//
		// This function is called to start the process of transmitting a packet.
		// We need to tell the channel that we've started wiggling the wire and
		// schedule an event that will be executed when the transmission is complete.
		//
		NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
		m_txMachineState = BUSY;
		m_currentPkt = p;
		m_phyTxBeginTrace(m_currentPkt);
		Time txTime = Seconds(m_bps.CalculateTxTime(p->GetSize()));
		Time txCompleteTime = txTime + m_tInterframeGap;
		NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds() << "sec");
		Simulator::Schedule(txCompleteTime, &QbbNetDevice::TransmitComplete, this);

		bool result = m_channel->TransmitStart(p, this, txTime);
		if (result == false)
		{
			m_phyTxDropTrace(p);
		}
		return result;
	}

	Ptr<Channel>
		QbbNetDevice::GetChannel(void) const
	{
		return m_channel;
	}

   bool QbbNetDevice::IsQbb(void) const{
	   return true;
   }

   void QbbNetDevice::NewQp(Ptr<RdmaQueuePair> qp){
	   qp->m_nextAvail = Simulator::Now();
	   DequeueAndTransmit();
   }
   void QbbNetDevice::ReassignedQp(Ptr<RdmaQueuePair> qp){
	   DequeueAndTransmit();
   }
   void QbbNetDevice::TriggerTransmit(void){
	   DequeueAndTransmit();
   }

	void QbbNetDevice::SetQueue(Ptr<BEgressQueue> q){
		NS_LOG_FUNCTION(this << q);
		m_queue = q;
	}

	Ptr<BEgressQueue> QbbNetDevice::GetQueue(){
		return m_queue;
	}

	Ptr<RdmaEgressQueue> QbbNetDevice::GetRdmaQueue(){
		return m_rdmaEQ;
	}

	void QbbNetDevice::RdmaEnqueueHighPrioQ(Ptr<Packet> p){
		m_traceEnqueue(p, 0);
		m_rdmaEQ->EnqueueHighPrioQ(p);
	}

	void QbbNetDevice::TakeDown(){
		// TODO: delete packets in the queue, set link down
		if (m_node->GetNodeType() == 0){
			// clean the high prio queue
			m_rdmaEQ->CleanHighPrio(m_traceDrop);
			// notify driver/RdmaHw that this link is down
			m_rdmaLinkDownCb(this);
		}else { // switch
			// clean the queue
			for (uint32_t i = 0; i < qCnt; i++)	m_paused[i] = false;
			while (1){
				Ptr<Packet> p = m_queue->DequeueRR(m_paused);
				if (p == 0) break;
				m_traceDrop(p, m_queue->GetLastQueue());
			}
			// TODO: Notify switch that this link is down
		}
		m_linkUp = false;
	}

	void QbbNetDevice::UpdateNextAvail(Time t){
		if (!m_nextSend.IsExpired() && (uint32_t)t.GetInteger() < m_nextSend.GetTs()){
			Simulator::Cancel(m_nextSend);
			Time delta = t < Simulator::Now() ? Time(0) : t - Simulator::Now();
			m_nextSend = Simulator::Schedule(delta, &QbbNetDevice::DequeueAndTransmit, this);
		}
	}
} // namespace ns3
