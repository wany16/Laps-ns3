#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/pause-header.h"
#include "ns3/flow-id-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "switch-node.h"
#include "qbb-net-device.h"
#include "ppp-header.h"
#include "ns3/int-header.h"
#include "ns3/simulator.h"
#include <cmath>
#include "ns3/ipv4-smartflow-tag.h"

namespace ns3
{

	NS_LOG_COMPONENT_DEFINE("SwitchNode");
	NS_OBJECT_ENSURE_REGISTERED(SwitchNode);
	std::map<uint32_t, std::map<uint32_t, uint32_t>> SwitchNode::m_rpsPortInf;
	std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, uint32_t>>> SwitchNode::m_ecmpPortInf;
	std::map<uint32_t, std::map<uint32_t, uint32_t>> SwitchNode::m_rrsPortInf;
	// LB_Solution SwitchNode::lbSolution = LB_Solution::NONE;
	uint32_t SwitchNode::GetHashValueFromCustomHeader(const CustomHeader &ch)
	{
		// pick one next hop based on hash
		union
		{
			uint8_t u8[4 + 4 + 2 + 2];
			uint32_t u32[3];
		} buf;
		buf.u32[0] = ch.sip;
		buf.u32[1] = ch.dip;
		if (ch.l3Prot == 0x6) // TCP
			buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
		else if (ch.l3Prot == 0x11) // UDP
			buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
		else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) // ACK or NACK
			buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
		else
		{
			std::cout << "Error in GetHashValueFromCustomHeader() for UNKOWN header type" << std::endl;
			return 0;
		}
		// no PFC and CNP
		uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed);
		return idx;
	}
	std::string SwitchNode::GetStringHashValueFromCustomHeader(const CustomHeader &ch)
	{
		uint16_t port = 0;
		if (ch.l3Prot == 0x6) // TCP
			port = ch.tcp.sport;
		else if (ch.l3Prot == 0x11) // UDP
			port = ch.udp.sport;
		else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) // ACK or NACK
			port = ch.ack.sport;
		else
		{
			std::cout << "Error in GetStringHashValueFromCustomHeader() for UNKOWN header type" << std::endl;
			return "";
		}
		std::string stringhash = std::to_string(ch.sip) + "#" + std::to_string(ch.dip) + "#" + std::to_string(port);
		return stringhash;
	}
	TypeId SwitchNode::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::SwitchNode")
								.SetParent<Node>()
								.AddConstructor<SwitchNode>()
								.AddAttribute("EcnEnabled",
											  "Enable ECN marking.",
											  BooleanValue(false),
											  MakeBooleanAccessor(&SwitchNode::m_ecnEnabled),
											  MakeBooleanChecker())
								.AddAttribute("CcMode",
											  "which mode of CC Algorithm is running on Switch.",
											  EnumValue(CongestionControlMode::CC_NONE),
											  MakeEnumAccessor(&SwitchNode::m_ccMode),
											  MakeEnumChecker(CongestionControlMode::DCQCN_MLX, "Dcqcn_mlx",
																				CongestionControlMode::HPCC_PINT, "Hpcc_pint",
																				CongestionControlMode::DCTCP, "Dctcp",
																				CongestionControlMode::TIMELY, "Timely",
																				CongestionControlMode::HPCC, "Hpcc",
																				CongestionControlMode::CC_NONE, "None"
																			 ))
								.AddAttribute("AckHighPrio",
											  "Set high priority for ACK/NACK or not",
											  UintegerValue(0),
											  MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
											  MakeUintegerChecker<uint32_t>())
								.AddAttribute("MaxRtt",
											  "Max Rtt of the network",
											  UintegerValue(9000),
											  MakeUintegerAccessor(&SwitchNode::m_maxRtt),
											  MakeUintegerChecker<uint32_t>())
								.AddAttribute("LbSolution", "Load balancing algorithm.",
											  EnumValue(LB_Solution::LB_NONE),
											  MakeEnumAccessor(&SwitchNode::m_lbSolution),
											  MakeEnumChecker(LB_Solution::LB_ECMP, "ecmp",
															  LB_Solution::LB_DRILL, "drill",
															  LB_Solution::LB_NONE, "none",
															  LB_Solution::LB_LETFLOW, "letflow",
															  LB_Solution::LB_CONGA, "conga",
															  LB_Solution::LB_DEFLOW, "deflow",
															  LB_Solution::LB_LAPS, "laps",
															  LB_Solution::LB_RPS, "rps",
															  LB_Solution::LB_RRS, "rrs"))
								.AddAttribute("d", "Load balancing algorithm drill: Sample d random outputs queue",
											  UintegerValue(2),
											  MakeUintegerAccessor(&SwitchNode::m_d),
											  MakeUintegerChecker<uint32_t>())
								.AddAttribute("FlowletTimeout", "The default flowlet timeout",
											  TimeValue(MicroSeconds(50)),
											  MakeTimeAccessor(&SwitchNode::m_flowletTimeout),
											  MakeTimeChecker());
		return tid;
	}

	SwitchNode::SwitchNode()
	{
		m_ecmpSeed = GetId();
		m_Seed = (uint32_t)Simulator::Now().GetNanoSeconds();
		m_node_type = 1;
		m_mmu = CreateObject<SwitchMmu>();
		m_d = 2;
		for (uint32_t i = 0; i < pCnt; i++)
			for (uint32_t j = 0; j < pCnt; j++)
				for (uint32_t k = 0; k < qCnt; k++)
					m_bytes[i][j][k] = 0;
		for (uint32_t i = 0; i < pCnt; i++)
			m_txBytes[i] = 0;
		for (uint32_t i = 0; i < pCnt; i++)
			m_lastPktSize[i] = m_lastPktTs[i] = 0;
		for (uint32_t i = 0; i < pCnt; i++)
			m_u[i] = 0;
	}

	CandidatePortEntry SwitchNode::GetEcmpRouteEntry(const Ipv4Address dip) const
	{
		auto it = m_ecmpRouteTable.find(dip);
		CandidatePortEntry ecmpEntry;
		if (it == m_ecmpRouteTable.end())
		{
			std::cout << "Error in GetAllCandidateEgressPorts(): found No matched routing entries" << std::endl;
		}
		else
		{
			ecmpEntry = it->second;
		}
		return ecmpEntry;
	}
	CandidatePortEntry SwitchNode::GetRpsRouteEntry(const Ipv4Address dip) const
	{
		auto it = m_rpsRouteTable.find(dip);
		CandidatePortEntry rpsEntry;
		if (it == m_ecmpRouteTable.end())
		{
			std::cout << "Error in GetAllCandidateEgressPorts(): found No matched routing entries" << std::endl;
		}
		else
		{
			rpsEntry = it->second;
		}
		return rpsEntry;
	}
	CandidatePortEntry SwitchNode::GetLetFlowRouteEntry(const Ipv4Address dip) const
	{
		auto it = m_letflowRouteTable.find(dip);
		CandidatePortEntry letflowEntry;
		if (it == m_letflowRouteTable.end())
		{
			std::cout << "Error in GetAllCandidateEgressPorts(): found No matched routing entries" << std::endl;
		}
		else
		{
			letflowEntry = it->second;
		}
		return letflowEntry;
	}
	RrsEntryInfo SwitchNode::GetRrsRouteEntry(const Ipv4Address dip) const
	{
		auto it = m_rrsRouteTable.find(dip);
		RrsEntryInfo rrsEntry;
		if (it == m_rrsRouteTable.end())
		{
			std::cout << "Error in GetAllCandidateEgressPorts(): found No matched routing entries" << std::endl;
		}
		else
		{
			rrsEntry = it->second;
		}
		return rrsEntry;
	}
	DrillEntryInfo SwitchNode::GetDrillRouteEntry(const Ipv4Address dip) const
	{
		auto it = m_drillRouteTable.find(dip);
		DrillEntryInfo drillEntry;
		if (it == m_drillRouteTable.end())
		{
			std::cout << "Error in GetAllCandidateEgressPorts(): found No matched routing entries" << std::endl;
		}
		else
		{
			drillEntry = it->second;
		}
		return drillEntry;
	}
	uint32_t SwitchNode::CalculateQueueLength(uint32_t interface)
	{ /*
	  uint32_t bufferload = 0;
	  for (uint32_t i = 0; i < qCnt; i++)
	  {
		  bufferload += m_mmu->egress_bytes[interface][i];
	  }
	  return bufferload;
	  */
		Ptr<Ipv4> ipv4 = GetObject<Ipv4>();
		Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(ipv4->GetNetDevice(interface));
		NS_ASSERT_MSG(!!device && !!device->GetQueue(), "Error of getting a egress queue for calculating interface load");
		return device->GetQueue()->GetNBytesTotal(); // also used in HPCC
	}

	uint32_t SwitchNode::GetDrillEgressPort(const Ipv4Address dip)
	{
		DrillEntryInfo drillEntry = GetDrillRouteEntry(dip);
		std::vector<uint32_t> allPorts = drillEntry.ports;
		if (allPorts.empty())
		{
			NS_LOG_ERROR(this << " Drill routing cannot find routing entry");
		}
		uint32_t leastLoadInterface = 0;
		uint32_t leastLoad = std::numeric_limits<uint32_t>::max();
		std::random_shuffle(allPorts.begin(), allPorts.end());
		std::map<uint32_t, uint32_t>::iterator itr = drillEntry.m_previousBestQueueMap.begin();
		if (itr != drillEntry.m_previousBestQueueMap.end())
		{
			leastLoadInterface = itr->first;
			leastLoad = CalculateQueueLength(itr->first);
		}
		uint32_t sampleNum = m_d < allPorts.size() ? m_d : allPorts.size();

		for (uint32_t samplePort = 0; samplePort < sampleNum; samplePort++)
		{
			uint32_t sampleLoad = CalculateQueueLength(allPorts[samplePort]);
			if (sampleLoad < leastLoad)
			{
				leastLoad = sampleLoad;
				leastLoadInterface = allPorts[samplePort];
			}
		}
		m_drillRouteTable[dip].m_previousBestQueueMap.clear();
		m_drillRouteTable[dip].m_previousBestQueueMap[leastLoadInterface] = leastLoad;

		if (GetId() == 3)
		{
			NS_LOG_INFO(GetId() << " Drill routing chooses interface: " << leastLoadInterface << ", since its load is: " << leastLoad << "byte");
		}
		return leastLoadInterface;
	}
	void SwitchNode::SetFlowletTimeout(Time timeout)
	{
		m_flowletTimeout = timeout;
	}
	uint32_t SwitchNode::GetLetFlowEgressPort(const Ipv4Address dip, std::string flowId)
	{
		CandidatePortEntry routeEntries = GetLetFlowRouteEntry(dip);
		uint32_t selectedPort;
		Time now = Simulator::Now();
		// If the flowlet table entry is valid, return the port
		std::map<std::string, struct LetFlowFlowletInfo>::iterator flowletItr = m_flowletTable.find(flowId);
		if (flowletItr != m_flowletTable.end())
		{
			LetFlowFlowletInfo flowlet = flowletItr->second;
			if (now - flowlet.activeTime <= m_flowletTimeout)
			{

				NS_LOG_INFO(GetId() << " hit flowletTable continue select the port: " << flowlet.port << ", since timegap is: " << (now - flowlet.activeTime).GetNanoSeconds() << "ns");
				// Do not forget to update the flowlet active time
				// if (GetId() == 6 && flowlet.port == 2)
				//{
				//	NS_LOG_INFO(GetId() << " hit flowletTable continue select the port: ");
				//}

				flowlet.activeTime = now;

				// Return the port information used for routing routine to select the port
				selectedPort = flowlet.port;

				m_flowletTable[flowId] = flowlet;

				return selectedPort;
			}
			else
			{
				NS_LOG_INFO(GetId() << " not hit flowletTable ,Random select the port" << ", since timegap is: " << (now - flowletItr->second.activeTime).GetNanoSeconds() << "ns");
			}
		}

		// Not hit. Random Select the Port
		selectedPort = routeEntries.ports[rand() % routeEntries.ports.size()];

		LetFlowFlowletInfo flowlet;

		flowlet.port = selectedPort;
		flowlet.activeTime = now;
		m_flowletTable[flowId] = flowlet;
		NS_LOG_INFO(" Random select the port is" << selectedPort << std::endl);
		return selectedPort;
	}
	int SwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch)
	{
		uint32_t swnodeid = GetId();
		switch (m_lbSolution)
		{
		case LB_Solution::LB_ECMP:
		{
			// NS_LOG_INFO("Apply Load Balancing Algorithm: " << "ECMP");
			uint32_t flowId = GetHashValueFromCustomHeader(ch);
			CandidatePortEntry ecmpEntry = GetEcmpRouteEntry(Ipv4Address(ch.dip));
			uint32_t egressPort = ecmpEntry.ports[flowId % ecmpEntry.ports.size()];
			if (m_ecmpPortInf[swnodeid][egressPort].find(flowId) != m_ecmpPortInf[swnodeid][egressPort].end())
			{
				m_ecmpPortInf[swnodeid][egressPort][flowId] += 1;
			}
			else
			{
				m_ecmpPortInf[swnodeid][egressPort][flowId] = 1;
			}
			return egressPort;
		}
		case LB_Solution::LB_DRILL:
		{
			NS_LOG_INFO("Apply Load Balancing Algorithm: " << "DRILL");
			// uint32_t flowId = GetHashValueFromCustomHeader(ch);
			uint32_t egressPort = GetDrillEgressPort(Ipv4Address(ch.dip));
			return egressPort;
		}
		case LB_Solution::LB_RPS:
		{
			NS_LOG_INFO("Apply Load Balancing Algorithm: " << "RPS");
			uint32_t randomNum = randomSelectionFromTime();
			CandidatePortEntry rpsEntry = GetRpsRouteEntry(Ipv4Address(ch.dip));
			uint32_t egressPort = rpsEntry.ports[randomNum % rpsEntry.ports.size()];

			m_rpsPortInf[swnodeid][egressPort] += 1;
			return egressPort;
		}
		case LB_Solution::LB_RRS:
		{
			NS_LOG_INFO("Apply Load Balancing Algorithm: " << "RRS");
			// uint32_t roundid = randomSelectionFromTime();
			RrsEntryInfo rrsEntry = GetRrsRouteEntry(Ipv4Address(ch.dip));
			uint32_t egressPort = rrsEntry.ports[(rrsEntry.current_index + 1) % rrsEntry.ports.size()];
			m_rrsRouteTable[Ipv4Address(ch.dip)].current_index += 1;
			m_rrsPortInf[swnodeid][egressPort] += 1;
			return egressPort;
		}
		case LB_Solution::LB_LETFLOW:
		{
			NS_LOG_INFO("Apply Load Balancing Algorithm: " << "LETFLOW");
			/*
			uint32_t flowId = 0;
			FlowIdTag flowIdTag;
			Ptr<Packet> packet = ConstCast<Packet>(p);
			bool flowIdFound = packet->PeekPacketTag(flowIdTag);
			if (!flowIdFound)
			{
				NS_LOG_ERROR(this << " LetFlow routing cannot extract the flow id");
			}
			flowId = flowIdTag.GetFlowId();
			*/
			// std::string flowId = std::to_string(ch.sip) + "#" + std::to_string(ch.dip) + std::to_string(ch.udp.sport);
			std::string flowId = GetStringHashValueFromCustomHeader(ch);
			uint32_t egressPort = GetLetFlowEgressPort(Ipv4Address(ch.dip), flowId);
			return egressPort;
		}

		default:
		{
			NS_FATAL_ERROR("Unknown LoadBalancingAlgorithm type");
			return 0;
		}
		}
		// look up entries
		auto entry = m_rtTable.find(ch.dip);

		// no matching entry
		if (entry == m_rtTable.end())
			return -1;

		// entry found
		auto &nexthops = entry->second;

		// pick one next hop based on hash
		union
		{
			uint8_t u8[4 + 4 + 2 + 2];
			uint32_t u32[3];
		} buf;
		buf.u32[0] = ch.sip;
		buf.u32[1] = ch.dip;
		if (ch.l3Prot == 0x6) // TCP
			buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
		else if (ch.l3Prot == 0x11) // UDP
			buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
		else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) // ACK or NACK
			buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
		// no PFC and CNP
		uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();
		return nexthops[idx];
	}

	// int SwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch){
	// 	// look up entries
	// 	auto entry = m_rtTable.find(ch.dip);

	// 	// no matching entry
	// 	if (entry == m_rtTable.end())
	// 		return -1;

	// 	// entry found
	// 	auto &nexthops = entry->second;

	// 	// pick one next hop based on hash
	// 	union {
	// 		uint8_t u8[4+4+2+2];
	// 		uint32_t u32[3];
	// 	} buf;
	// 	buf.u32[0] = ch.sip;
	// 	buf.u32[1] = ch.dip;
	// 	if (ch.l3Prot == 0x6) // TCP
	// 		buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
	// 	else if (ch.l3Prot == 0x11) // UDP
	// 		buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
	// 	else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) // ACK or NACK
	// 		buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
	// 	// no PFC and CNP
	// 	uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();
	// 	return nexthops[idx];
	// }

	void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex)
	{
		Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(GetDevice(inDev));
		if (m_mmu->CheckShouldPause(inDev, qIndex))
		{
			device->SendPfc(qIndex, 0);
			m_mmu->SetPause(inDev, qIndex);
		}
	}
	void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex)
	{
		Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(GetDevice(inDev));
		if (m_mmu->CheckShouldResume(inDev, qIndex))
		{
			device->SendPfc(qIndex, 1);
			m_mmu->SetResume(inDev, qIndex);
		}
	}

	void SwitchNode::SendToDev(Ptr<Packet> p, CustomHeader &ch)
	{
		int idx = GetOutDev(p, ch);
		if (idx >= 0)
		{
			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(GetDevice(idx));

			NS_ASSERT_MSG(dev->IsLinkUp(), "The routing table look up should return link that is up");

			// determine the qIndex
			uint32_t qIndex;
			if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC)))
			{ // QCN or PFC or NACK, go highest priority
				qIndex = 0;
			}
			else
			{
				qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // if TCP, put to queue 1
			}

			// admission control
			FlowIdTag t;
			p->PeekPacketTag(t);
			uint32_t inDev = t.GetFlowId();
			if (qIndex != 0)
			{ // not highest priority
				if (m_mmu->CheckIngressAdmission(inDev, qIndex, p->GetSize()) && m_mmu->CheckEgressAdmission(idx, qIndex, p->GetSize()))
				{ // Admission control
					m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize());
					m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize());
				}
				else
				{
					return; // Drop
				}
				CheckAndSendPfc(inDev, qIndex);
			}
			m_bytes[inDev][idx][qIndex] += p->GetSize();

			dev->SwitchSend(qIndex, p, ch);
		}
		else
			return; // Drop
	}

	uint32_t SwitchNode::EcmpHash(const uint8_t *key, size_t len, uint32_t seed)
	{
		uint32_t h = seed;
		if (len > 3)
		{
			const uint32_t *key_x4 = (const uint32_t *)key;
			size_t i = len >> 2;
			do
			{
				uint32_t k = *key_x4++;
				k *= 0xcc9e2d51;
				k = (k << 15) | (k >> 17);
				k *= 0x1b873593;
				h ^= k;
				h = (h << 13) | (h >> 19);
				h += (h << 2) + 0xe6546b64;
			} while (--i);
			key = (const uint8_t *)key_x4;
		}
		if (len & 3)
		{
			size_t i = len & 3;
			uint32_t k = 0;
			key = &key[i - 1];
			do
			{
				k <<= 8;
				k |= *key--;
			} while (--i);
			k *= 0xcc9e2d51;
			k = (k << 15) | (k >> 17);
			k *= 0x1b873593;
			h ^= k;
		}
		h ^= len;
		h ^= h >> 16;
		h *= 0x85ebca6b;
		h ^= h >> 13;
		h *= 0xc2b2ae35;
		h ^= h >> 16;
		return h;
	}
	uint32_t SwitchNode::randomSelectionFromTime()
	{
		unsigned int Seed = static_cast<unsigned int>(Simulator::Now().GetNanoSeconds() % 4294967295);
		SetSeed(Seed);
		std::srand(Seed);
		return static_cast<uint32_t>(std::rand());
	}
	void SwitchNode::SetEcmpSeed(uint32_t seed)
	{
		m_ecmpSeed = seed;
	}
	void SwitchNode::SetSeed(uint32_t seed)
	{
		m_Seed = seed;
	}
	void SwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx)
	{
		uint32_t dip = dstAddr.Get();
		m_rtTable[dip].push_back(intf_idx);
		if (m_lbSolution == LB_Solution::LB_ECMP)
		{
			m_ecmpRouteTable[dstAddr].ports.push_back(intf_idx);
		}
		if (m_lbSolution == LB_Solution::LB_RPS)
		{
			m_rpsRouteTable[dstAddr].ports.push_back(intf_idx);
		}
		if (m_lbSolution == LB_Solution::LB_RRS)
		{
			m_rrsRouteTable[dstAddr].ports.push_back(intf_idx);
		}
		if (m_lbSolution == LB_Solution::LB_DRILL)
		{
			m_drillRouteTable[dstAddr].ports.push_back(intf_idx);
		}
		if (m_lbSolution == LB_Solution::LB_LETFLOW)
		{
			m_letflowRouteTable[dstAddr].ports.push_back(intf_idx);
		}
	}

	void SwitchNode::ClearTable()
	{
		m_rtTable.clear();
	}

	// This function can only be called in switch mode
	bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch)
	{
		SendToDev(packet, ch);
		return true;
	}

	void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p)
	{
		FlowIdTag t;
		p->PeekPacketTag(t);
		if (qIndex != 0)
		{
			uint32_t inDev = t.GetFlowId();
			m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize());
			m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize());
			m_bytes[inDev][ifIndex][qIndex] -= p->GetSize();
			if (m_ecnEnabled)
			{
				bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
				if (egressCongested)
				{
					PppHeader ppp;
					Ipv4Header h;
					p->RemoveHeader(ppp);
					p->RemoveHeader(h);
					h.SetEcn((Ipv4Header::EcnType)0x03);
					p->AddHeader(h);
					p->AddHeader(ppp);
				}
			}
			// CheckAndSendPfc(inDev, qIndex);
			CheckAndSendResume(inDev, qIndex);
		}
		if (1)
		{
			uint8_t *buf = p->GetBuffer();
			if (buf[PppHeader::GetStaticSize() + 9] == 0x11)
			{																				// udp packet
				IntHeader *ih = (IntHeader *)&buf[PppHeader::GetStaticSize() + 20 + 8 + 6]; // ppp, ip, udp, SeqTs, INT
				Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(GetDevice(ifIndex));
				if (m_ccMode == CongestionControlMode::HPCC)
				{ // HPCC
					ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
				}
				else if (m_ccMode == CongestionControlMode::HPCC_PINT)
				{ // HPCC-PINT
					uint64_t t = Simulator::Now().GetTimeStep();
					uint64_t dt = t - m_lastPktTs[ifIndex];
					if (dt > m_maxRtt)
						dt = m_maxRtt;
					uint64_t B = dev->GetDataRate().GetBitRate() / 8; // Bps
					uint64_t qlen = dev->GetQueue()->GetNBytesTotal();
					double newU;

					/**************************
					 * approximate calc
					 *************************/
					int b = 20, m = 16, l = 20; // see log2apprx's paremeters
					int sft = logres_shift(b, l);
					double fct = 1 << sft;				 // (multiplication factor corresponding to sft)
					double log_T = log2(m_maxRtt) * fct; // log2(T)*fct
					double log_B = log2(B) * fct;		 // log2(B)*fct
					double log_1e9 = log2(1e9) * fct;	 // log2(1e9)*fct
					double qterm = 0;
					double byteTerm = 0;
					double uTerm = 0;
					if ((qlen >> 8) > 0)
					{
						int log_dt = log2apprx(dt, b, m, l);		  // ~log2(dt)*fct
						int log_qlen = log2apprx(qlen >> 8, b, m, l); // ~log2(qlen / 256)*fct
						qterm = pow(2, (
										   log_dt + log_qlen + log_1e9 - log_B - 2 * log_T) /
										   fct) *
								256;
						// 2^((log2(dt)*fct+log2(qlen/256)*fct+log2(1e9)*fct-log2(B)*fct-2*log2(T)*fct)/fct)*256 ~= dt*qlen*1e9/(B*T^2)
					}
					if (m_lastPktSize[ifIndex] > 0)
					{
						int byte = m_lastPktSize[ifIndex];
						int log_byte = log2apprx(byte, b, m, l);
						byteTerm = pow(2, (
											  log_byte + log_1e9 - log_B - log_T) /
											  fct);
						// 2^((log2(byte)*fct+log2(1e9)*fct-log2(B)*fct-log2(T)*fct)/fct) ~= byte*1e9 / (B*T)
					}
					if (m_maxRtt > dt && m_u[ifIndex] > 0)
					{
						int log_T_dt = log2apprx(m_maxRtt - dt, b, m, l);				 // ~log2(T-dt)*fct
						int log_u = log2apprx(int(round(m_u[ifIndex] * 8192)), b, m, l); // ~log2(u*512)*fct
						uTerm = pow(2, (
										   log_T_dt + log_u - log_T) /
										   fct) /
								8192;
						// 2^((log2(T-dt)*fct+log2(u*512)*fct-log2(T)*fct)/fct)/512 = (T-dt)*u/T
					}
					newU = qterm + byteTerm + uTerm;

#if 0
				/**************************
				 * accurate calc
				 *************************/
				double weight_ewma = double(dt) / m_maxRtt;
				double u;
				if (m_lastPktSize[ifIndex] == 0)
					u = 0;
				else{
					double txRate = m_lastPktSize[ifIndex] / double(dt); // B/ns
					u = (qlen / m_maxRtt + txRate) * 1e9 / B;
				}
				newU = m_u[ifIndex] * (1 - weight_ewma) + u * weight_ewma;
				printf(" %lf\n", newU);
#endif

					/************************
					 * update PINT header
					 ***********************/
					uint16_t power = Pint::encode_u(newU);
					if (power > ih->GetPower())
						ih->SetPower(power);

					m_u[ifIndex] = newU;
				}
			}
		}
		m_txBytes[ifIndex] += p->GetSize();
		m_lastPktSize[ifIndex] = p->GetSize();
		m_lastPktTs[ifIndex] = Simulator::Now().GetTimeStep();
	}

	int SwitchNode::logres_shift(int b, int l)
	{
		static int data[] = {0, 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
		return l - data[b];
	}

	int SwitchNode::log2apprx(int x, int b, int m, int l)
	{
		int x0 = x;
		int msb = int(log2(x)) + 1;
		if (msb > m)
		{
			x = (x >> (msb - m) << (msb - m));
#if 0
		x += + (1 << (msb - m - 1));
#else
			int mask = (1 << (msb - m)) - 1;
			if ((x0 & mask) > (rand() & mask))
				x += 1 << (msb - m);
#endif
		}
		return int(log2(x) * (1 << logres_shift(b, l)));
	}

} /* namespace ns3 */
