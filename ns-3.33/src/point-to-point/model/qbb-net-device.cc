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
	std::map<std::string, std::string> QbbNetDevice::qpSendInfo;
  RandomIntegerGenerator QbbNetDevice::pktCorruptRandGen = RandomIntegerGenerator(2, 0.00001);
	bool QbbNetDevice::isEnableRndPktLoss = false;

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
		if (qIndex == int(QINDEX_OF_ACK_PACKET_IN_SERVER)){ // high prio
			Ptr<Packet> p = m_ackQ->Dequeue();
			m_qlast = QINDEX_OF_ACK_PACKET_IN_SERVER;
			m_traceRdmaDequeue(p, 0);
			return p;
		}
		if (qIndex >= 0){ // qp
			Ptr<Packet> p = m_rdmaGetNxtPkt(m_qpGrp->Get(qIndex));
			m_qpGrp->Get(qIndex)->sendDateSize += p->GetSize();
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
			// if (RdmaHw::m_recordQpExec.find(qp->m_flow_id) == RdmaHw::m_recordQpExec.end())
			// {
			// 	std::cerr << "FlowId " << qp->m_flow_id << " is in m_recordQpExec" << std::endl;
			// 	exit(1);
			// }
			// RdmaHw::m_recordQpExec[qp->m_flow_id].lastVistTime = Simulator::Now().GetNanoSeconds();
			// RdmaHw::m_recordQpExec[qp->m_flow_id].nxtAvailTime = qp->m_nextAvail.GetNanoSeconds();
			if (qp->IsFinishedConst()) m_qpGrp->SetQpFinished(idx);
      if (m_qpGrp->IsQpFinished(idx))
			{
				// RdmaHw::m_recordQpExec[qp->m_flow_id].pauseReason = "beSetFinished";
				// RdmaHw::m_recordQpExec[qp->m_flow_id].snd_una = qp->snd_una;
				// RdmaHw::m_recordQpExec[qp->m_flow_id].snd_nxt = qp->snd_nxt;
				continue;
			}
			bool isPfcAllowed = !paused[qp->m_pg];
			int32_t flowid = qp->m_flow_id;
			auto it = RdmaHw::m_recordQpExec.find(flowid);
			if (isPfcAllowed) 
			{
				if (it->second.lastPfcPauseTime != -1)
				{
					uint64_t tdiff = Simulator::Now().GetNanoSeconds() - it->second.lastPfcPauseTime;
					it->second.pfcDuration += tdiff;
					it->second.lastPfcPauseTime = -1;
				}
			}
			else
			{
				if (it->second.lastPfcPauseTime == -1)
				{
					it->second.lastPfcPauseTime = Simulator::Now().GetNanoSeconds();
				}
			}
			


			// if (!isPfcAllowed) RdmaHw::m_recordQpExec[qp->m_flow_id].pauseReason = "+PfcPause";
			bool isWinAllowed = !qp->IsWinBound();
			// if (!isWinAllowed) RdmaHw::m_recordQpExec[qp->m_flow_id].pauseReason = "+WinBound";
			bool isIrnAllowed = qp->CanIrnTransmit(mtuInByte);
			// if (!isIrnAllowed) RdmaHw::m_recordQpExec[qp->m_flow_id].pauseReason = "+IrnLimit";
			bool isDataLeft = qp->GetBytesLeft() > 0 ? true : false;
			// if (!isDataLeft) RdmaHw::m_recordQpExec[qp->m_flow_id].pauseReason = "+ZoroDataLeft";
			bool isTimeAvail = qp->m_nextAvail.GetTimeStep() > Simulator::Now().GetTimeStep() ? false : true;
			// if (!isTimeAvail) RdmaHw::m_recordQpExec[qp->m_flow_id].pauseReason = "+TimeNotAvail";
			// if (qp->snd_una == 0 && qp->snd_nxt == 0)
			// {
			// 	std::cout << qp->GetStringHashValueFromQp() << " curtime " << Simulator::Now().GetNanoSeconds();
			// 	std::cout << " FLOWId " << flowid << " SIZE " << qp->m_size << " una " << qp->snd_una << " Pfc " << isPfcAllowed << " Win " << isWinAllowed << " irn " << isIrnAllowed << " Data " << isDataLeft << " isTime " << isTimeAvail << std::endl;
			// 	std::ostringstream oss;
			// 	oss << " curtime " << Simulator::Now().GetNanoSeconds();
			// 	oss << " FLOWId " << flowid << " SIZE " << qp->m_size << " una " << qp->snd_una << " Pfc " << isPfcAllowed << " Win " << isWinAllowed << " irn " << isIrnAllowed << " Data " << isDataLeft << " isTime " << isTimeAvail;
			// 	QbbNetDevice::qpSendInfo[qp->GetStringHashValueFromQp()] = oss.str();
			// }
			if (!isPfcAllowed && isDataLeft && isWinAllowed && isIrnAllowed) {
					if (!isTimeAvail) { // not available now
					} else {// blocked by PFC
							// if (!MAP_KEY_EXISTS(m_startPauseTime, flowid)) m_startPauseTime[flowid] = Simulator::Now();
							// auto it = RdmaHw::m_recordQpExec.find(flowid);
							// if (it->second.lastPfcPauseTime == -1)
							// {
							// 	it->second.lastPfcPauseTime = Simulator::Now().GetNanoSeconds();
							// }
					}
			}else if (isPfcAllowed && isDataLeft && isWinAllowed && isIrnAllowed) {
            if (!isTimeAvail)  continue; // not available now
						// if (MAP_KEY_EXISTS(m_startPauseTime, flowid)) {             // Check if the flow has been blocked by PFC
						// 		Time tdiff = Simulator::Now() - m_startPauseTime[flowid];
						// 		if (!MAP_KEY_EXISTS(m_startPauseTime, flowid)) cumulative_pause_time[flowid] = Seconds(0);
						// 		cumulative_pause_time[flowid] += tdiff;
						// 		m_startPauseTime.erase(flowid);
						// }
						// auto it = RdmaHw::m_recordQpExec.find(flowid);
						// if (it->second.lastPfcPauseTime != -1)
						// {
						// 	uint64_t tdiff = Simulator::Now().GetNanoSeconds() - it->second.lastPfcPauseTime;
						// 	it->second.pfcDuration += tdiff;
						// 	it->second.lastPfcPauseTime = -1;
						// }
						// RdmaHw::m_recordQpExec[qp->m_flow_id].lastSelectedTime = Simulator::Now().GetNanoSeconds();
						// RdmaHw::m_recordQpExec[qp->m_flow_id].pauseReason = "Not Paused";
            return idx;
      }
		}
		return QINDEX_OF_NONE_PACKET_IN_SERVER;
	}

	int RdmaEgressQueue::GetNextQindexOnHostForLaps(bool paused[]){ // only for host
		NS_LOG_FUNCTION(this);
		NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT || Irn::mode == Irn::Mode::NACK, "Called Only When Laps is enabled");
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
			// if(!qp->m_cb_isPathsValid(qp->m_flow_id)) continue;
			// qp->CheckAndUpdateQpStateForLaps();
			bool isPfcAllowed = !paused[qp->m_pg];
			int32_t flowid = qp->m_flow_id;
			auto it = RdmaHw::m_recordQpExec.find(flowid);
			if (isPfcAllowed) 
			{
				if (it->second.lastPfcPauseTime != -1)
				{
					uint64_t tdiff = Simulator::Now().GetNanoSeconds() - it->second.lastPfcPauseTime;
					it->second.pfcDuration += tdiff;
					it->second.lastPfcPauseTime = -1;
				}
			}
			else
			{
				if (it->second.lastPfcPauseTime == -1)
				{
					it->second.lastPfcPauseTime = Simulator::Now().GetNanoSeconds();
				}
			}

			bool isWinAllowed = !qp->IsWinBoundForLaps();
			bool isIrnAllowed = qp->CanIrnTransmitForLaps(mtuInByte);
			bool isDataLeft = qp->GetBytesLeftForLaps() > 0 ? true : false;
			bool isTimeAvail = qp->m_nextAvail.GetTimeStep() > Simulator::Now().GetTimeStep() ? false : true;
			// int32_t flowid = qp->m_flow_id;

	  if (!isPfcAllowed && isDataLeft && isWinAllowed && isIrnAllowed)
	  {
		  if (isTimeAvail)
		  { // blocked by PFC
				// auto it = RdmaHw::m_recordQpExec.find(flowid);
				// if (it->second.lastPfcPauseTime == -1)
				// {
				// 	it->second.lastPfcPauseTime = Simulator::Now().GetNanoSeconds();
				// }
		  }
	  }
			else if (isPfcAllowed && isDataLeft && isWinAllowed && isIrnAllowed)
			{
				if (!isTimeAvail)  continue; // not available now
				// auto it = RdmaHw::m_recordQpExec.find(flowid);
				// if (it->second.lastPfcPauseTime != -1)
				// {
				// 	uint64_t tdiff = Simulator::Now().GetNanoSeconds() - it->second.lastPfcPauseTime;
				// 	it->second.pfcDuration += tdiff;
				// 	it->second.lastPfcPauseTime = -1;
				// }
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
											  MakeEnumChecker(LB_Solution::LB_E2ELAPS, "e2elaps",
															  LB_Solution::LB_PLB, "plb"));

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
	// bool
	// QbbNetDevice::LbPacketTransmitStart(Ptr<E2ESrcOutPackets> &srcOutEntryPtr)
	// {

	// 	//
	// 	// This function is called to start the process of transmitting a packet.
	// 	// We need to tell the channel that we've started wiggling the wire and
	// 	// schedule an event that will be executed when the transmission is complete.
	// 	//

	// 	NS_LOG_FUNCTION(this);
	// 	NS_ASSERT_MSG(m_node->GetNodeType() == NODE_TYPE_OF_SERVER, "Must be Called on Source Host");
	// 	NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
	// 	m_txMachineState = BUSY;

	// 	Time txTime_base = Seconds(0);
	// 	uint32_t totalPktSize = 0;
	// 	Time totalTInterframeGap = Seconds(0);

	// 	if (srcOutEntryPtr->Isprobe)
	// 	{
	// 		Ptr<Packet> prbPkt = srcOutEntryPtr->probePacket;
	// 		m_currentPkt = prbPkt;
	// 		m_phyTxBeginTrace(m_currentPkt);
	// 		Time txTime_PrbPkt = Seconds(m_bps.CalculateTxTime(prbPkt->GetSize()));
	// 		Time txCompleteTime_PrbPkt = txTime_PrbPkt + m_tInterframeGap;
	// 		bool result_PrbPkt = m_channel->TransmitStart(prbPkt, this, txTime_PrbPkt);
	// 		if (result_PrbPkt == false)
	// 		{
	// 			m_phyTxDropTrace(prbPkt);
	// 			std::cout << "Transmit Probing Packet failed" << std::endl;
	// 			exit(1);
	// 		}
	// 		txTime_base += txCompleteTime_PrbPkt;
	// 		totalPktSize += prbPkt->GetSize();
	// 		totalTInterframeGap += m_tInterframeGap;
	// 	}

	// 	Ptr<Packet> p = srcOutEntryPtr->dataPacket;
	// 	m_currentPkt = p;
	// 	m_phyTxBeginTrace(m_currentPkt);
	// 	Time txTime_DataPkt = Seconds(m_bps.CalculateTxTime(p->GetSize()));
	// 	Time txCompleteTime_DataPkt = txTime_DataPkt + m_tInterframeGap;
	// 	bool result_DataPkt = m_channel->TransmitStart(p, this, txTime_DataPkt+txTime_base);
	// 	if (result_DataPkt == false)
	// 	{
	// 		m_phyTxDropTrace(p);
	// 		std::cout << "Transmit Data Packet failed" << std::endl;
	// 		exit(1);
	// 	}
	// 	totalPktSize += p->GetSize();
	// 	totalTInterframeGap += m_tInterframeGap;
	// 	Simulator::Schedule(txCompleteTime_DataPkt+txTime_base, &QbbNetDevice::TransmitComplete, this);
	// 	if (!srcOutEntryPtr->Isack)
	// 	{
	// 		m_rdmaLbPktSent(srcOutEntryPtr->lastQp, totalPktSize, totalTInterframeGap);
	// 	}
		
	// 	return true;

		
	// 	// probe = srcOutEntryPtr->probePacket;
	// 	// Time txTime_DataPkt = Seconds(m_bps.CalculateTxTime(p->GetSize()));
	// 	// Time txCompleteTime_DataPkt = txTime_DataPkt + m_tInterframeGap;
	// 	// bool result_DataPkt = m_channel->TransmitStart(p, this, txCompleteTime_DataPkt);
	// 	// if (result_DataPkt == false)
	// 	// {
	// 	// 	m_phyTxDropTrace(p);
	// 	// 	std::cout << "QbbNetDevice::LbPacketTransmitStart: TransmitStart failed" << std::endl;
	// 	// 	exit(1);
	// 	// }


	// 	// Time allInterframeGap = m_tInterframeGap;
	// 	// uint32_t allPktSize = p->GetSize();
	// 	// if (srcOutEntryPtr->Isprobe) // 遗留bug,探测报文不能正常发送；
	// 	// {
	// 	// 	probe = srcOutEntryPtr->probePacket;
	// 	// 	Time txTime2 = Seconds(m_bps.CalculateTxTime(probe->GetSize()));
	// 	// 	txCompleteTime += (txTime2 + m_tInterframeGap);
	// 	// 	bool result1 = m_channel->TransmitStart(probe, this, txTime2);
	// 	// 	if (result1 == false)
	// 	// 	{
	// 	// 		m_phyTxDropTrace(p);
	// 	// 	}
	// 	// 	NS_LOG_LOGIC("Probe packet transmitCompleteEvent in " << txTime2.GetSeconds() << "sec");
	// 	// 	allInterframeGap += m_tInterframeGap;
	// 	// 	allPktSize += probe->GetSize();
	// 	// }
	// 	// NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds() << "sec");
	// 	// Simulator::Schedule(txCompleteTime, &QbbNetDevice::TransmitComplete, this);

	// 	// bool result = m_channel->TransmitStart(p, this, txTime1);
	// 	// if (result == false)
	// 	// {
	// 	// 	m_phyTxDropTrace(p);
	// 	// }

	// 	// // int qIndex = m_rdmaEQ->GetNextQindex(m_paused);
	// 	// if (!srcOutEntryPtr->Isack)
	// 	// {
	// 	// 	m_rdmaLbPktSent(srcOutEntryPtr->lastQp, allPktSize, allInterframeGap);
	// 	// }

	// 	// return result;
	// }


	bool
	QbbNetDevice::TransmitStartOnSrcHostForLaps(Ptr<E2ESrcOutPackets> entry)
	{

		//
		// This function is called to start the process of transmitting a packet.
		// We need to tell the channel that we've started wiggling the wire and
		// schedule an event that will be executed when the transmission is complete.
		//

		NS_LOG_FUNCTION(this);
		NS_LOG_INFO("#Node: " << m_node->GetId() << ", Time: " << Simulator::Now().GetNanoSeconds() <<	" ns, Start send out Packets");

		NS_ASSERT_MSG(m_node->GetNodeType() == NODE_TYPE_OF_SERVER, "Must be Called on Source Host");
		NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
		NS_ASSERT_MSG(entry && (entry->isAck ^ entry->isData), "Must be either Ack or Data Packet");
		m_txMachineState = BUSY;

		Time txTime_base = Seconds(0);
		uint32_t totalPktSize = 0;
		Time totalTInterframeGap = Seconds(0);
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		uint32_t hdr_size = 0;
		uint16_t payloadSize = 0;

		if (entry->isProbe)
		{
			m_currentPkt = entry->probePacket;
			m_phyTxBeginTrace(m_currentPkt);
			Time txTime = Seconds(m_bps.CalculateTxTime(m_currentPkt->GetSize()));
			Time txCompleteTime = txTime + m_tInterframeGap;
			bool result_PrbPkt = m_channel->TransmitStart(m_currentPkt, this, txTime);
			if (result_PrbPkt == false)
			{
				m_phyTxDropTrace(m_currentPkt);
				std::cerr << "Transmit Probing Packet failed" << std::endl;
			}
			txTime_base += txCompleteTime;
			totalPktSize += m_currentPkt->GetSize();
			totalTInterframeGap += m_tInterframeGap;

			Ipv4SmartFlowPathTag pathTag;
			m_currentPkt->PeekPacketTag(pathTag);
			uint32_t pid = pathTag.get_path_id();
			uint64_t pktId = m_currentPkt->GetUid();
			m_currentPkt->PeekHeader(ch);
			hdr_size = ch.GetSerializedSize();
			payloadSize = m_currentPkt->GetSize()-hdr_size;
			NS_LOG_INFO("PktId: " << pktId  << " Type: Probe, Size: " << payloadSize <<	" Pid: " << pid);
		}

		if (entry->isData)
		{
			m_currentPkt = entry->dataPacket;
			m_phyTxBeginTrace(m_currentPkt);
			Time txTime = Seconds(m_bps.CalculateTxTime(m_currentPkt->GetSize()));
			Time txCompleteTime = txTime + m_tInterframeGap;
			bool result_DataPkt = m_channel->TransmitStart(m_currentPkt, this, txTime_base + txTime);
			if (result_DataPkt == false)
			{
				m_phyTxDropTrace(m_currentPkt);
				std::cerr << "Transmit Data Packet failed" << std::endl;
				exit(1);
			}
			totalPktSize += m_currentPkt->GetSize();
			totalTInterframeGap += m_tInterframeGap;
			Simulator::Schedule(txCompleteTime+txTime_base, &QbbNetDevice::TransmitComplete, this);
			NS_ASSERT_MSG(entry->lastQp, "Invalid QP");
			m_rdmaLbPktSent(entry->lastQp, totalPktSize, totalTInterframeGap);
			uint32_t fid = entry->lastQp->m_flow_id;
			RdmaHw::m_recordQpExec[fid].sendSizeInbyte += m_currentPkt->GetSize() - ch.GetSerializedSize();
			RdmaHw::m_recordQpExec[fid].sendPacketNum++;

			Ipv4SmartFlowPathTag pathTag;
			m_currentPkt->PeekPacketTag(pathTag);
			uint32_t pid = pathTag.get_path_id();
			ch.getInt = 1; // parse INT header
			uint64_t pktId = m_currentPkt->GetUid();
			m_currentPkt->PeekHeader(ch);
			hdr_size = ch.GetSerializedSize();
			payloadSize = m_currentPkt->GetSize()-hdr_size;
			NS_LOG_INFO("PktId: " << pktId  << " Type: DATA, Size: " << payloadSize <<	" Pid: " << pid);
      // entry->lastQp->m_irn.m_sack.appendOutstandingData(pid, ch.udp.seq, payloadSize); /////////////////////////////
			// std::cout << "PktId: " << pktId << " Type: DATA, Size: " << payloadSize << " Pid: " << pid << std::endl;
			auto e = OutStandingDataEntry(entry->lastQp->m_flow_id, ch.udp.seq, payloadSize);

			m_rdmaOutStanding_cb(pid, e);
			return true;
		}
		else if (entry->isAck)
		{
			m_currentPkt = entry->ackPacket;
			m_phyTxBeginTrace(m_currentPkt);
			Time txTime = Seconds(m_bps.CalculateTxTime(m_currentPkt->GetSize()));
			Time txCompleteTime = txTime + m_tInterframeGap;
			bool result_AckPkt = m_channel->TransmitStart(m_currentPkt, this, txTime_base + txTime);
			if (result_AckPkt == false)
			{
				m_phyTxDropTrace(m_currentPkt);
				std::cerr << "Transmit Ack Packet failed" << std::endl;
				exit(1);
			}
			totalPktSize += m_currentPkt->GetSize();
			totalTInterframeGap += m_tInterframeGap;
			Simulator::Schedule(txCompleteTime+txTime_base, &QbbNetDevice::TransmitComplete, this);

			Ipv4SmartFlowPathTag pathTag;
			m_currentPkt->PeekPacketTag(pathTag);
			uint32_t pathId = pathTag.get_path_id();

			AckPathTag ackTag;
			m_currentPkt->PeekPacketTag(ackTag);
			uint32_t rPathPid = ackTag.GetPathId();
			uint64_t pktId = m_currentPkt->GetUid();
			m_currentPkt->PeekHeader(ch);
			hdr_size = ch.GetSerializedSize();
			payloadSize = m_currentPkt->GetSize()-hdr_size;
			NS_LOG_INFO("PktId: " << pktId  << " Type: ACK, Size: " << payloadSize <<	" fPid: " << pathId << " rPid: " << rPathPid);

			return true;
		}

		else
		{
			NS_ASSERT_MSG(false, "Invalid entry");
			return false;
		}

	}


	void QbbNetDevice::UpdateNxtDequeueAndTransmitTimeOnSrcHostForLaps()
	{ 
		NS_LOG_FUNCTION(this << "Node=" << m_node->GetId());
		Time t = Simulator::GetMaximumSimulationTime();
		bool valid = false;
		for (uint32_t i = 0; i < m_rdmaEQ->GetFlowCount(); i++)
		{
			Ptr<RdmaQueuePair> qp = m_rdmaEQ->GetQp(i);
			if (qp->GetBytesLeftForLaps() == 0)	continue;
			// Time t1 = qp->m_cb_getNxtAvailTimeForQp(qp->m_flow_id);
			// qp->m_nextAvail = std::max(qp->m_nextAvail, t1);
			// bool ispathavail = qp->m_cb_isPathsValid(qp->m_flow_id);
			// if (!ispathavail)
			// {
			// 	qp->m_nextAvail = qp->m_nextAvail + NanoSeconds(5000);
			// }
			t = Min(qp->m_nextAvail, t);
			valid = true;
		}


		if (valid && m_nextSend.IsExpired() && t < Simulator::GetMaximumSimulationTime() && t > Simulator::Now())
		{
			m_nextSend = Simulator::Schedule(t - Simulator::Now(), &QbbNetDevice::DequeueAndTransmit, this);
		}
	}

	Ptr<E2ESrcOutPackets> QbbNetDevice::GetTransmitQpContentOnSrcHostForLaps(int32_t qpFlowIndex)
	{
		NS_LOG_FUNCTION(this << "Node=" << m_node->GetId() << "qpFlowIndex=" << qpFlowIndex);
		NS_ASSERT_MSG(qpFlowIndex != int(QINDEX_OF_NONE_PACKET_IN_SERVER), "Invalid qpFlowIndex");

		Ptr<E2ESrcOutPackets> entry = Create<E2ESrcOutPackets>();
		if (qpFlowIndex == int(QINDEX_OF_ACK_PACKET_IN_SERVER))
		{
			entry->isAck = true;
			entry->ackPacket = m_rdmaEQ->DequeueQindex(qpFlowIndex);
			m_traceDequeue(entry->ackPacket, 0);
		}
		else if (qpFlowIndex >= 0)
		{
			entry->isData = true;
			entry->lastQp = m_rdmaEQ->GetQp(qpFlowIndex);
			entry->dataPacket = m_rdmaEQ->DequeueQindex(qpFlowIndex);
			m_traceQpDequeue(entry->dataPacket, entry->lastQp);
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid qpFlowIndex");
		}
		return entry;
	}

	void QbbNetDevice::AddPathTagOnSrcHostForLaps(Ptr<E2ESrcOutPackets> entry)
	{
		NS_LOG_FUNCTION(this << "Node=" << m_node->GetId());
		NS_ASSERT_MSG(entry && (entry->isData ^ entry->isAck), "dataPacket and ackPacket are the same");

		Ptr<RdmaSmartFlowRouting> m_routing = m_rdmaGetE2ELapsLBouting();

		if (entry->isData)
		{
			m_routing->RouteOutputForDataPktOnSrcHostForLaps(entry);
		}
		else if (entry->isAck)
		{
			m_routing->RouteOutputForAckPktOnSrcHostForLaps(entry);
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid entry");
		}
	}

	void QbbNetDevice::UpdatePathTagOnSrcHostForLaps(Ptr<E2ESrcOutPackets> entry)
	{

		NS_LOG_FUNCTION(this);
		// NS_LOG_INFO("#Node: " << m_node->GetId() << ", Time: " << Simulator::Now().GetNanoSeconds() <<	" ns, Action: Update PathTag on Src Host for LAPS");
		NS_ASSERT_MSG(entry && (entry->isData ^ entry->isAck), "dataPacket or ackPacket, not both");

		Ptr<RdmaSmartFlowRouting> m_routing = m_rdmaGetE2ELapsLBouting();
		Ipv4SmartFlowPathTag pathTag;

		if (entry->isData)
		{
			// NS_LOG_INFO("Type: DATA, PktId: "<< entry->dataPacket->GetUid() << ", Size: " << entry->dataPacket->GetSize());
			bool IsHavePathTag = m_routing->exist_path_tag(entry->dataPacket, pathTag);
			NS_ASSERT_MSG(IsHavePathTag, "PathTag does not exist on DATA packet");
			m_routing->update_path_tag(entry->dataPacket, pathTag);
		}
		else if (entry->isAck)
		{
			// NS_LOG_INFO("Type: ACK, PktId: "<< entry->ackPacket->GetUid() << ", Size: " << entry->ackPacket->GetSize());
			bool ishaveAckTag = m_routing->exist_path_tag(entry->ackPacket, pathTag);
			NS_ASSERT_MSG(ishaveAckTag, "PathTag does not exist on ACK packet");
			m_routing->update_path_tag(entry->ackPacket, pathTag);
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid entry");
		}

		if (entry->isProbe)
		{
			// NS_LOG_INFO("Type: PROBE, PktId: "<< entry->probePacket->GetUid() << ", Size: " << entry->probePacket->GetSize());
			bool IsHaveProbeTag = m_routing->exist_path_tag(entry->probePacket, pathTag);
			NS_ASSERT_MSG(IsHaveProbeTag, "PathTag does not exist on Probe packet");
			m_routing->update_path_tag(entry->probePacket, pathTag);
		}
	}
	bool QbbNetDevice::PLB_LBSolution(int qIndex)
	{
		NS_LOG_INFO("******PLB add Rehashtag******");
		bool Isack;
		Ptr<Packet> p;
		PlbRehashTag plbtag;
		// extract customheader
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
						CustomHeader::L4_Header);
		if (qIndex == QINDEX_OF_ACK_PACKET_IN_SERVER)
		{
			p = m_rdmaEQ->DequeueQindex(qIndex);
			if (p->PeekHeader(ch) > 0)
			{
				NS_LOG_INFO("QbbNetDevice::ApplyLoadBalancingSolution Extracted CustomHeader:");
			}
			std::string stringhash = ipv4Address2string(Ipv4Address(ch.dip)) + "#" + ipv4Address2string(Ipv4Address(ch.sip)) + "#" + std::to_string(ch.ack.sport); // srcPort=dstPort
			// NS_LOG_INFO("stringhash is " << stringhash);
			uint32_t randNum = m_plbTableDataCb(stringhash);
			// std::string flowId = stringhash;
			// RdmaHw::m_recordQpExec[flowId].sendAckInbyte += p->GetSize();
			// RdmaHw::m_recordQpExec[flowId].sendAckPacketNum++;

			plbtag.SetRandomNum(randNum);
			p->AddPacketTag(plbtag);
			m_traceDequeue(p, 0);
			TransmitStart(p);
			Isack = true;
			return Isack;
		}

		Ptr<RdmaQueuePair> lastQp = m_rdmaEQ->GetQp(qIndex);
		p = m_rdmaEQ->DequeueQindex(qIndex);
		if (p->PeekHeader(ch) > 0)
		{
			NS_LOG_INFO("QbbNetDevice::ApplyLoadBalancingSolution Extracted CustomHeader:");
		}
		else
		{
			std::cerr << "PLB: cannot find custom header" << std::endl;
			exit(1);
		}
		uint32_t flowId = lastQp->m_flow_id;
		auto it = RdmaHw::m_recordQpExec.find(flowId);
		if (it == RdmaHw::m_recordQpExec.end())
		{
			std::cerr << "PLB: cannot find flowId" << std::endl;
			exit(1);
		}
		else
		{
			it->second.sendSizeInbyte += p->GetSize() - ch.GetSerializedSize();
			it->second.sendPacketNum++;
		}
		std::string stringhash = ipv4Address2string(Ipv4Address(ch.sip)) + "#" + ipv4Address2string(Ipv4Address(ch.dip)) + "#" + std::to_string(ch.udp.sport);
		// std::string flowId = stringhash;
		// RdmaHw::m_recordQpExec[flowId].sendSizeInbyte += p->GetSize();
		// RdmaHw::m_recordQpExec[flowId].sendPacketNum++;
		// NS_LOG_INFO("stringhash is " << stringhash);
		// RdmaHw::m_recordQpExec[flowId].sendSizeInbyte += p->GetSize() - ch.GetSerializedSize();;
		// RdmaHw::m_recordQpExec[flowId].sendPacketNum++;
		uint32_t randNum = m_plbTableDataCb(stringhash);
		plbtag.SetRandomNum(randNum);
		p->AddPacketTag(plbtag);
		m_traceQpDequeue(p, lastQp);
		TransmitStart(p);
		m_rdmaPktSent(lastQp, p, m_tInterframeGap);
		Isack = false;
		return Isack;
	}

	/*bool QbbNetDevice::PLB_LBSolution(int qIndex)
	{
		NS_LOG_INFO("******PLB add Rehashtag******")
		bool Isack;
		Ptr<Packet> p;
		// extract customheader
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
						CustomHeader::L4_Header);
		PlbRehashTag plbtag;
		if (qIndex == QINDEX_OF_ACK_PACKET_IN_SERVER)
		{
			p = m_rdmaEQ->DequeueQindex(qIndex);
			if (p->PeekHeader(ch) > 0)
			{
				NS_LOG_INFO("QbbNetDevice::ApplyLoadBalancingSolution Extracted CustomHeader:");
			}

			m_traceDequeue(p, 0);
			TransmitStart(p);
			Isack = true;
			return Isack;
		}
		// extract customheader
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
						CustomHeader::L4_Header);
		if (p->PeekHeader(ch) > 0)
		{
			NS_LOG_INFO("QbbNetDevice::ApplyLoadBalancingSolution Extracted CustomHeader:");
		}

		std::string stringhash = ipv4Address2string(sip) + "#" + ipv4Address2string(dip) + "#" + std::to_string(sport); // srcPort=dstPort

		Ptr<RdmaQueuePair> lastQp = m_rdmaEQ->GetQp(qIndex);
		p = m_rdmaEQ->DequeueQindex(qIndex);
		uint32_t randNum = lastQp->flowRandNum;
		PlbRehashTag plbtag;
		plbtag.SetRandomNum(randNum);
		p->AddPacketTag(plbtag);
		m_traceQpDequeue(p, lastQp);
		TransmitStart(p);
		m_rdmaPktSent(lastQp, p, m_tInterframeGap);
		Isack = false;
		return Isack;
	}*/

	void
	QbbNetDevice::DequeueAndTransmit(void)
	{
		NS_LOG_FUNCTION(this << "Node=" << m_node->GetId());
		if (m_node->GetNodeType() == NODE_TYPE_OF_SERVER && m_lbSolution == LB_Solution::LB_E2ELAPS)
		{
			DequeueAndTransmitOnSrcHostForLAPS();
			return;
		}

		
		if (!m_linkUp) return; // if link is down, return
		if (m_txMachineState == BUSY) return;	// Quit if channel busy
		Ptr<Packet> p;
		// NS_LOG_INFO("----------------MyNode:" << m_node->GetId());

		if (m_node->GetNodeType() == NODE_TYPE_OF_SERVER)
		{													// for netdevice in host, has 8 virtual queues
			int qIndex = m_rdmaEQ->GetNextQindex(m_paused); // the index of the qp (NOT queue or virtual queue) to send next
			if (qIndex != int(QINDEX_OF_NONE_PACKET_IN_SERVER))
			{
				// exist packet to send
				if (m_lbSolution == LB_Solution::LB_PLB)
				{
					PLB_LBSolution(qIndex);
					return;
				}
				else
				{
					if (qIndex == QINDEX_OF_ACK_PACKET_IN_SERVER)
					{
						// extract customheader
						CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
										CustomHeader::L4_Header);

						// exist ack packets in the highest priority queue
						p = m_rdmaEQ->DequeueQindex(QINDEX_OF_ACK_PACKET_IN_SERVER); // get the actual packet to send
						p->PeekHeader(ch);
						m_traceDequeue(p, 0);										 // trace the current packet
						TransmitStart(p);											 // start the sending action

						// std::string stringhash = ipv4Address2string(Ipv4Address(ch.dip)) + "#" + ipv4Address2string(Ipv4Address(ch.sip)) + "#" + std::to_string(ch.ack.sport); // srcPort=dstPort

						// std::string flowId = stringhash;
						// RdmaHw::m_recordQpExec[flowId].sendAckInbyte += p->GetSize();
						// RdmaHw::m_recordQpExec[flowId].sendAckPacketNum++;

						return;
					}
					// no ack packet in the highest priority queue, so to process the qIndex-th queue
					Ptr<RdmaQueuePair> lastQp = m_rdmaEQ->GetQp(qIndex); // to dequeue a packet in a RR manner
					// uint32_t flowId = lastQp->m_flow_id;
					p = m_rdmaEQ->DequeueQindex(qIndex);
					CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
									CustomHeader::L4_Header);
					p->PeekHeader(ch);
					// std::string stringhash = ipv4Address2string(Ipv4Address(ch.sip)) + "#" + ipv4Address2string(Ipv4Address(ch.dip)) + "#" + std::to_string(ch.udp.sport);
					// std::string flowId = stringhash;
					// RdmaHw::m_recordQpExec[flowId].sendSizeInbyte += p->GetSize();
					// RdmaHw::m_recordQpExec[flowId].sendPacketNum++;
					// transmit
					m_traceQpDequeue(p, lastQp);
					TransmitStart(p);
					uint32_t flowId = lastQp->m_flow_id;
					auto it = RdmaHw::m_recordQpExec.find(flowId);
					if (it == RdmaHw::m_recordQpExec.end())
					{
						std::cerr << "PLB: cannot find flowId" << std::endl;
						exit(1);
					}
					else
					{
						it->second.sendSizeInbyte += p->GetSize() - ch.GetSerializedSize();
						it->second.sendPacketNum++;
					}
					// update for the next avail time
					m_rdmaPktSent(lastQp, p, m_tInterframeGap);
				}
			}
			else // no packet to send
			{ 
				Time t = Simulator::GetMaximumSimulationTime();
				bool valid = false;
				for (uint32_t i = 0; i < m_rdmaEQ->GetFlowCount(); i++)
				{
					Ptr<RdmaQueuePair> qp = m_rdmaEQ->GetQp(i);
					if (qp->GetBytesLeft() == 0)	continue;
					t = Min(qp->m_nextAvail, t);
					valid = true;
				}
				if (valid && m_nextSend.IsExpired() && t < Simulator::GetMaximumSimulationTime() && t > Simulator::Now())
				{
					m_nextSend = Simulator::Schedule(t - Simulator::Now(), &QbbNetDevice::DequeueAndTransmit, this);
				}
				// if (valid && m_nextSend.IsExpired() && t < Simulator::GetMaximumSimulationTime())
				// {
				// 	if (t > Simulator::Now())
				// 	{
				// 		m_nextSend = Simulator::Schedule(t - Simulator::Now(), &QbbNetDevice::DequeueAndTransmit, this);
				// 	}
				// 	else
				// 	{
				// 		m_nextSend = Simulator::Schedule(MicroSeconds(5), &QbbNetDevice::DequeueAndTransmit, this);
				// 	}
				// }
			}
			return;
		}
		else // switch, doesn't care about qcn, just send
		{ 
			p = m_queue->DequeueRR(m_paused); 
			if (p != 0)
			{
				m_snifferTrace(p);
				m_promiscSnifferTrace(p);
				// Ipv4Header h;
				// Ptr<Packet> packet = p->Copy();
				// uint16_t protocol = 0;
				// ProcessHeader(packet, protocol);
				// packet->RemoveHeader(h);
				uint32_t qIndex = m_queue->GetLastQueue();
				DynamicCast<SwitchNode>(m_node)->SwitchNotifyDequeue(m_ifIndex, qIndex, p);
				CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
				ch.getInt = 1; // parse INT header
				p->PeekHeader(ch);
				FlowIdTag t;
				NS_ASSERT_MSG(ch.l3Prot == L3ProtType::PFC || p->RemovePacketTag(t), "FlowIdTag does not exist on non-PFC packet traveling through switch");
				m_traceDequeue(p, qIndex);
				TransmitStart(p);
				return;
			}
			else
			{ // No queue can deliver any packet
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
		}
		return;
	}

	void
	QbbNetDevice::DequeueAndTransmitOnSrcHostForLAPS()
	{
		NS_LOG_FUNCTION(this << "Node=" << m_node->GetId());
		NS_ASSERT_MSG(m_lbSolution == LB_Solution::LB_E2ELAPS, "LB Solution Must be LB_E2ELAPS");
		NS_ASSERT_MSG(Irn::mode == Irn::Mode::NACK || Irn::mode == Irn::Mode::IRN_OPT, "LAPS::NACK should be enabled");
		NS_ASSERT_MSG(m_node->GetNodeType() == NODE_TYPE_OF_SERVER, "Must be Called on Source Host");
		if (!m_linkUp) return; // if link is down, return
		if (m_txMachineState == BUSY) return;	// Quit if channel busy
		
		int qpFlowIndex = m_rdmaEQ->GetNextQindexOnHostForLaps(m_paused);
		if (qpFlowIndex == int(QINDEX_OF_NONE_PACKET_IN_SERVER))
		{
			UpdateNxtDequeueAndTransmitTimeOnSrcHostForLaps();
		}
		else
		{
			Ptr<E2ESrcOutPackets> outEntry = GetTransmitQpContentOnSrcHostForLaps(qpFlowIndex);
			AddPathTagOnSrcHostForLaps(outEntry);
			// if (outEntry->isData)
			// {
			// 	m_rtoSetCb(outEntry->lastQp, outEntry->pidForDataPkt, NanoSeconds(2*outEntry->latencyForDataPktInNs));
			// }
			UpdatePathTagOnSrcHostForLaps(outEntry);
			TransmitStartOnSrcHostForLaps(outEntry);
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
	if (isEnableRndPktLoss && pktCorruptRandGen.GetUniformInt() == 0)
	{
			m_phyRxDropTrace(packet);
			std::cout << "Node " << m_node->GetId() << " dev " << m_ifIndex << " drop packet at " << Simulator::Now().GetSeconds() << std::endl;
			return;
	}

		m_macRxTrace(packet);
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
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
				bool IsHaveTag = packet->PeekPacketTag(flowIdTag);
				NS_ASSERT_MSG(!IsHaveTag, "FlowIdTag already exists on packet traveling through switch");
				flowIdTag.SetFlowId(m_ifIndex);
				packet->AddPacketTag(flowIdTag);
				DynamicCast<SwitchNode>(m_node)->SwitchReceiveFromDevice(this, packet, ch);
			}
			else
			{ // NIC
				if (m_lbSolution == LB_Solution::LB_E2ELAPS)
				{
					Ptr<RdmaSmartFlowRouting> m_routing = m_rdmaGetE2ELapsLBouting();
					bool ShouldUpForward = m_routing->RouteInput(packet, ch);
					if (ShouldUpForward)
					{
						m_rdmaReceiveCb(packet, ch);
					}
				}
				else
				{
					m_rdmaReceiveCb(packet, ch);
				}
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
		// std::cout << "pfc enable" << m_qbbEnabled;
		if (!m_qbbEnabled) return 0;
		Ptr<Packet> p = Create<Packet>(0);
		Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
		PauseHeader pauseh((type == PfcPktType::PAUSE ? m_pausetime : 0), m_queue->GetNBytes(qIndex), qIndex);
		if (type == PfcPktType::PAUSE)
		{
			NS_LOG_INFO("TimeInNs: " << Simulator::Now().GetNanoSeconds() << " Node: " << m_node->GetId() << " Nic: " << m_ifIndex << " Qindex: " << qIndex << " Send PAUSE");
			// std::cout << "TimeInNs: " << Simulator::Now().GetNanoSeconds() << " Node: " << m_node->GetId() << " Nic: " << m_ifIndex << " Qindex: " << qIndex << " Send PAUSE" << std::endl;
		}else{
			NS_LOG_INFO("TimeInNs: " << Simulator::Now().GetNanoSeconds() << " Node: " << m_node->GetId() << " Nic: " << m_ifIndex << " Qindex: " << qIndex << " ");
			// std::cout << "TimeInNs: " << Simulator::Now().GetNanoSeconds() << " Node: " << m_node->GetId() << " Nic: " << m_ifIndex << " Qindex: " << qIndex << " " << std::endl;
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
		 uint32_t flowId = qp->m_flow_id;

		 auto it = RdmaHw::m_recordQpExec.find(flowId);
		 if (it == RdmaHw::m_recordQpExec.end())
		 {
				QpRecordEntry qpRecordEntry;
				qpRecordEntry.flowsize = qp->m_size;
				qpRecordEntry.flowId = flowId;
				qpRecordEntry.installTime = Simulator::Now().GetNanoSeconds();
				qpRecordEntry.srcNodeId = m_node->GetId();
				qpRecordEntry.windowSize = qp->m_win;
				qpRecordEntry.initialRate = qp->m_rate.GetBitRate() / 1000000000;
			 	RdmaHw::m_recordQpExec[flowId] = qpRecordEntry;
				QpRecordEntry::installFlowCnt++;
		 }
		 else
		 {
			std::cerr << "QbbNetDevice::NewQp: flowId already exists" << std::endl;
			exit(1);
		 }
	 
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
