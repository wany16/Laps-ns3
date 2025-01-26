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
#include "assert.h"

namespace ns3
{

	NS_LOG_COMPONENT_DEFINE("SwitchNode");
	NS_OBJECT_ENSURE_REGISTERED(SwitchNode);
	std::map<uint32_t, std::map<uint32_t, uint32_t>> SwitchNode::m_rpsPortInf;
	std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, uint32_t>>> SwitchNode::m_ecmpPortInf;
	std::map<uint32_t, std::map<uint32_t, uint32_t>> SwitchNode::m_rrsPortInf;
	std::map<uint32_t, std::map<uint32_t, SwitchNode::FlowPortInfo>> SwitchNode::m_PortInf;
	RoutePath SwitchNode::routePath;
	std::map<uint32_t, std::map<uint64_t, std::string>> SwitchNode::congaoutinfo;
	std::map<uint32_t, std::map<std::string, std::map<uint64_t, letflowSaveEntry>>> SwitchNode::m_letflowTestInf;
	std::map<HostId2PathSeleKey, std::map<uint32_t, std::map<uint32_t,std::vector<uint64_t>>>> SwitchNode::m_recordPath;
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
		/*
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
		}*/
		// no PFC and CNP
		buf.u32[2] = ch.udp.sport;
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
																CongestionControlMode::CC_LAPS, "Laps",
															  CongestionControlMode::CC_NONE, "None"))
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
															  LB_Solution::LB_CONWEAVE, "conweave",
															  LB_Solution::LB_PLB, "plb",
															  LB_Solution::LB_DEFLOW, "deflow",
															  LB_Solution::LB_LAPS, "laps",
															  LB_Solution::LB_RPS, "rps",
															  LB_Solution::LB_E2ELAPS, "e2elaps",
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
		m_isToR = false;
		m_switch_id = GetId();

		// set constants
		m_dreTime = Time(MicroSeconds(200));
		m_agingTime = Time(MilliSeconds(10));
		m_flowletTimeout = Time(MicroSeconds(10));
		m_quantizeBit = 3;
		m_alpha = 0.2;
		Ptr<RdmaSmartFlowRouting> smartFlowRouting = m_mmu->m_SmartFlowRouting;

		// LAPS OR e2eLaps Callback for switch functions
		m_mmu->m_SmartFlowRouting->SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
		m_mmu->m_SmartFlowRouting->SetSwitchSendToDevCallback(MakeCallback(&SwitchNode::SendToDevContinue, this));
		m_mmu->m_SmartFlowRouting->SetSwitchInfo(m_isToR, m_switch_id);
		m_mmu->m_SmartFlowRouting->SetNode(this);

		// Ptr<ConWeaveRouting> conweaverouting = m_mmu->m_ConWeaveRouting;

		// ConWeaveRouting Callback for switch functions
		m_mmu->m_ConWeaveRouting->SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
		m_mmu->m_ConWeaveRouting->SetSwitchSendToDevCallback(MakeCallback(&SwitchNode::SendToDevContinue, this));
		m_mmu->m_ConWeaveRouting->SetSwitchInfo(m_isToR, m_switch_id);
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
	/* Conga-Tag */
	CongaTag::CongaTag()
	{
	}
	CongaTag::~CongaTag() {}
	TypeId CongaTag::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::CongaTag").SetParent<Tag>().AddConstructor<CongaTag>();
		return tid;
	}
	void CongaTag::SetPathId(uint32_t pathId) { m_pathId = pathId; }
	uint32_t CongaTag::GetPathId(void) const { return m_pathId; }
	void CongaTag::SetCe(uint32_t ce) { m_ce = ce; }
	uint32_t CongaTag::GetCe(void) const { return m_ce; }
	void CongaTag::SetFbPathId(uint32_t fbPathId) { m_fbPathId = fbPathId; }
	uint32_t CongaTag::GetFbPathId(void) const { return m_fbPathId; }
	void CongaTag::SetFbMetric(uint32_t fbMetric) { m_fbMetric = fbMetric; }
	uint32_t CongaTag::GetFbMetric(void) const { return m_fbMetric; }

	void CongaTag::SetHopCount(uint32_t hopCount) { m_hopCount = hopCount; }
	uint32_t CongaTag::GetHopCount(void) const { return m_hopCount; }
	TypeId CongaTag::GetInstanceTypeId(void) const { return GetTypeId(); }
	uint32_t CongaTag::GetSerializedSize(void) const
	{
		return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) +
			   sizeof(uint32_t);
	}
	void CongaTag::Serialize(TagBuffer i) const
	{
		i.WriteU32(m_pathId);
		i.WriteU32(m_ce);
		i.WriteU32(m_hopCount);
		i.WriteU32(m_fbPathId);
		i.WriteU32(m_fbMetric);
	}
	void CongaTag::Deserialize(TagBuffer i)
	{
		m_pathId = i.ReadU32();
		m_ce = i.ReadU32();
		m_hopCount = i.ReadU32();
		m_fbPathId = i.ReadU32();
		m_fbMetric = i.ReadU32();
	}
	void CongaTag::Print(std::ostream &os) const
	{
		os << "m_pathId=" << m_pathId;
		os << ", m_ce=" << m_ce;
		os << ", m_hopCount=" << m_hopCount;
		os << ". m_fbPathId=" << m_fbPathId;
		os << ", m_fbMetric=" << m_fbMetric;
	}
	/*----- Conga-LB ------*/
	uint32_t SwitchNode::nFlowletTimeout = 0;

	void SwitchNode::SetSwitchInfo(bool isToR, uint32_t switch_id)
	{
		m_isToR = isToR;
		m_switch_id = switch_id;
		m_mmu->m_ConWeaveRouting->SetSwitchInfo(isToR, switch_id);
	}
	bool SwitchNode::GetIsToRSwitch()
	{
		return m_isToR;
	}
	uint32_t SwitchNode::GetSwitchId()
	{
		return m_switch_id;
	}
	void SwitchNode::SetLinkCapacity(uint32_t outPort, uint64_t bitRate)
	{
		auto it = m_outPort2BitRateMap.find(outPort);
		if (it != m_outPort2BitRateMap.end())
		{
			// already exists, then check matching
			NS_ASSERT_MSG(it->second == bitRate,
						  "bitrate already exists, but inconsistent with new input");
		}
		else
		{
			m_outPort2BitRateMap[outPort] = bitRate;
		}
	}
	bool SwitchNode::reach_the_last_hop_of_path_tag(CongaTag congaTag)
	{
		uint32_t pathId = congaTag.GetPathId();
		PathData *pitEntry = routePath.lookup_PIT(pathId);
		uint32_t pathSize = pitEntry->portSequence.size();
		uint32_t hopIdx = congaTag.GetHopCount() + 1;
		if (hopIdx == pathSize - 2)
		{
			NS_LOG_INFO("Reaching the Dst ToR");
			return true;
		}
		else
		{
			NS_LOG_INFO("Is " << hopIdx << "/" << (pathSize - 2) << " hops");
			return false;
		}
	}
	uint32_t SwitchNode::GetCongaBestPath(HostId2PathSeleKey forwarPstKey, uint32_t nSample)
	{
		pstEntryData *PstEntry = routePath.lookup_PST(forwarPstKey);
		std::vector<PathData *> forwardPitEntries;
		if (PstEntry->pathNum < nSample)
		{
			forwardPitEntries = routePath.batch_lookup_PIT(PstEntry->paths);
		}
		else
		{
			uint32_t rndPathIdStartValue = std::rand() % (PstEntry->pathNum - nSample + 1);
			for (uint32_t i = 0; i < nSample; i++)
			{
				// uint32_t rndPathIdx = std::rand() % forwardPathNum;
				//  std::cout << "The " << i << "-th path index is " << rndPathIdx << std::endl;
				uint32_t rndPathId = PstEntry->paths[rndPathIdStartValue];
				PathData *rndPitEntry = routePath.lookup_PIT(rndPathId);
				forwardPitEntries.push_back(rndPitEntry);
				rndPathIdStartValue++;
			}
		}

		std::vector<uint32_t> candidatePaths;
		uint32_t minCongestion = CONGA_NULL;
		uint32_t roundIdx = 0;
		for (auto it = forwardPitEntries.begin(); it != forwardPitEntries.end(); ++it)
		{
			PathData *curPitEntry = *it;
			// no info means good
			uint32_t localCongestion = 0;
			uint32_t remoteCongestion = 0;

			auto outPort = GetOutPortFromPath(curPitEntry->pid, 0);
			auto innerDre = m_DreMap.find(outPort);
			if (innerDre != m_DreMap.end())
			{
				localCongestion = QuantizingX(outPort, innerDre->second);
			}

			// remote congestion
			remoteCongestion = curPitEntry->pathDre;
			// get maximum of congestion (local, remote)
			uint32_t CurrCongestion = std::max(localCongestion, remoteCongestion);
			NS_LOG_INFO("GetCongaBestPath" << " roundIdx " << roundIdx << " minCongestion " << minCongestion << " localCongestion " << localCongestion << "remoteCongestion" << remoteCongestion << "\n");
			// filter the best path
			if (minCongestion > CurrCongestion)
			{
				minCongestion = CurrCongestion;
				candidatePaths.clear();
				candidatePaths.push_back(curPitEntry->pid); // best
			}
			else if (minCongestion == CurrCongestion)
			{
				candidatePaths.push_back(curPitEntry->pid); // equally good
			}
			NS_LOG_INFO("GetCongaBestPath" << " roundIdx " << roundIdx << " CurrCongestion" << CurrCongestion << "/n");
			roundIdx++;
		}
		assert(candidatePaths.size() > 0 && "candidatePaths has no entry");
		return candidatePaths[std::rand() % candidatePaths.size()]; // randomly choose the best path
	}

	// minimize the maximum link utilization
	uint32_t SwitchNode::GetBestPath(uint32_t dstToRId, uint32_t nSample)
	{
		auto pathItr = m_congaRoutingTable.find(dstToRId);
		assert(pathItr != m_congaRoutingTable.end() && "Cannot find dstToRId from ToLeafTable");
		std::set<uint32_t>::iterator innerPathItr = pathItr->second.begin();
		if (pathItr->second.size() >= nSample)
		{ // exception handling
			std::advance(innerPathItr, rand() % (pathItr->second.size() - nSample + 1));
		}
		else
		{
			nSample = pathItr->second.size();
			NS_LOG_INFO("WARNING - Conga's number of path sampling is higher than available paths.Enforced to reduce nSample:" << nSample);
		}

		// path info for remote congestion, <pathId -> pathInfo>
		auto pathInfoMap = m_congaToLeafTable[dstToRId];

		// get min-max path
		std::vector<uint32_t> candidatePaths;
		uint32_t minCongestion = CONGA_NULL;
		for (uint32_t i = 0; i < nSample; i++)
		{
			// get info of path
			uint32_t pathId = *innerPathItr;
			auto innerPathInfo = pathInfoMap.find(pathId);

			// no info means good
			uint32_t localCongestion = 0;
			uint32_t remoteCongestion = 0;

			auto outPort = GetOutPortFromPath(pathId, 0); // outPort from pathId (TxToR)

			// local congestion -> get Port Util and quantize it
			auto innerDre = m_DreMap.find(outPort);
			if (innerDre != m_DreMap.end())
			{
				localCongestion = QuantizingX(outPort, innerDre->second);
			}

			// remote congestion
			if (innerPathInfo != pathInfoMap.end())
			{
				remoteCongestion = innerPathInfo->second._ce;
			}

			// get maximum of congestion (local, remote)
			uint32_t CurrCongestion = std::max(localCongestion, remoteCongestion);

			// filter the best path
			if (minCongestion > CurrCongestion)
			{
				minCongestion = CurrCongestion;
				candidatePaths.clear();
				candidatePaths.push_back(pathId); // best
			}
			else if (minCongestion == CurrCongestion)
			{
				candidatePaths.push_back(pathId); // equally good
			}
			std::advance(innerPathItr, 1);
		}
		assert(candidatePaths.size() > 0 && "candidatePaths has no entry");
		return candidatePaths[rand() % candidatePaths.size()]; // randomly choose the best path
	}

	uint32_t SwitchNode::UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort)
	{
		uint32_t X = m_DreMap[outPort];
		uint32_t newX = X + p->GetSize();
		NS_LOG_INFO("Old X " << X << " New X " << newX << " outPort " << outPort << " Switch " << m_switch_id << " TimeInNS " << Simulator::Now());
		m_DreMap[outPort] = newX;
		return newX;
	}

	uint32_t SwitchNode::GetOutPortFromPath(const uint32_t &path, const uint32_t &hopCount)
	{
		PathData *PitEntry = routePath.lookup_PIT(path);

		return PitEntry->portSequence[hopCount + 1]; // Is e2e path,Set hopnum+1
	}

	void SwitchNode::SetOutPortToPath(uint32_t &path, const uint32_t &hopCount,
									  const uint32_t &outPort)
	{
		((uint8_t *)&path)[hopCount] = outPort;
	}

	uint32_t SwitchNode::QuantizingX(uint32_t outPort, uint32_t X)
	{
		auto it = m_outPort2BitRateMap.find(outPort);
		assert(it != m_outPort2BitRateMap.end() && "Cannot find bitrate of interface");
		uint64_t bitRate = it->second;
		double ratio = static_cast<double>(X * 8) / (bitRate * m_dreTime.GetSeconds() / m_alpha);
		uint32_t quantX = static_cast<uint32_t>(ratio * std::pow(2, m_quantizeBit));
		if (quantX > 3)
		{
			NS_LOG_INFO("X " << X << " Ratio " << ratio << "Bits" << quantX << Simulator::Now());
		}
		return quantX;
	}

	void SwitchNode::SetConstants(Time dreTime, Time agingTime, uint32_t quantizeBit, double alpha)
	{
		m_dreTime = dreTime;
		m_agingTime = agingTime;
		m_quantizeBit = quantizeBit;
		m_alpha = alpha;
	}
	/*
	 void SwitchNode::DoDispose()
	 {
		 for (auto i : m_flowletTable)
		 {
			 delete (i.second);
		 }
		 m_dreEvent.Cancel();
		 m_agingEvent.Cancel();
	 }*/

	void SwitchNode::DreEvent()
	{
		std::map<uint32_t, uint32_t>::iterator itr = m_DreMap.begin();
		for (; itr != m_DreMap.end(); ++itr)
		{
			uint32_t newX = itr->second * (1 - m_alpha);
			itr->second = newX;
		}

		NS_LOG_INFO(Simulator::Now());
		m_dreEvent = Simulator::Schedule(m_dreTime, &SwitchNode::DreEvent, this);
	}

	void SwitchNode::AgingEvent()
	{
		auto now = Simulator::Now();
		auto itr = routePath.m_nexthopSelTbl.begin(); // always non-empty
		for (; itr != routePath.m_nexthopSelTbl.end(); ++itr)
		{

			if (now - (itr->second).updateTime > m_agingTime)
			{
				(itr->second).pathDre = 0;
			}
		}

		/*auto itr2 = m_congaFromLeafTable.begin();
		while (itr2 != m_congaFromLeafTable.end())
		{
			auto innerItr2 = (itr2->second).begin();
			while (innerItr2 != (itr2->second).end())
			{
				if (now - (innerItr2->second)._updateTime > m_agingTime)
				{
					innerItr2 = (itr2->second).erase(innerItr2);
				}
				else
				{
					++innerItr2;
				}
			}
			++itr2;
		}*/

		auto itr3 = m_congaflowletTable.begin();
		while (itr3 != m_congaflowletTable.end())
		{
			if (now - ((itr3->second)->activeTime) > m_agingTime)
			{
				// delete(itr3->second); // delete pointer
				itr3 = m_congaflowletTable.erase(itr3);
			}
			else
			{
				++itr3;
			}
		}
		NS_LOG_INFO(Simulator::Now());
		m_agingEvent = Simulator::Schedule(m_agingTime, &SwitchNode::AgingEvent, this);
	}

	void SwitchNode::RecordPathload()
	{

		for (auto it = routePath.m_pathSelTbl.begin(); it != routePath.m_pathSelTbl.end(); it++)
		{
			HostId2PathSeleKey key = it->first;
			std::vector<uint32_t> paths = it->second.paths;
			// 确保 m_recordPath 中有对应的 key 和时间戳
			if (m_recordPath.find(key) == m_recordPath.end())
			{
				m_recordPath[key] = std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>>();
			}
			if (m_recordPath[key].find(m_recordTime.GetMilliSeconds()) == m_recordPath[key].end())
			{
				m_recordPath[key][m_recordTime.GetMilliSeconds()] = std::map<uint32_t, std::vector<uint64_t>>();
			}
			for (uint32_t path : paths)
			{
				PathData *pathinfo = routePath.lookup_PIT(path);
				uint64_t pathloadgap = pathinfo->pathload - pathinfo->lastpathload;
				pathinfo->lastpathload = pathinfo->pathload;
				m_recordPath[key][m_recordTime.GetMilliSeconds() ][path].push_back(pathloadgap) ;
			}
		}
		recordNum++;
		m_recordEvent = Simulator::Schedule(m_recordTime, &SwitchNode::RecordPathload, this);
	}
	void SwitchNode::updatePathLoad(uint32_t size, uint32_t pathId)
	{
		PathData *pitEntry = routePath.lookup_PIT(pathId);
		pitEntry->pathload += size;
		return;
	}
	uint32_t SwitchNode::GetCongaEgressPort(Ptr<Packet> p, CustomHeader &ch)
	{

		CongaTag congaTag;
		uint32_t selectedPort;
		// Packet arrival time
		Time now = Simulator::Now();

		// Turn on DRE event scheduler if it is not running
		if (!m_dreEvent.IsRunning())
		{
			NS_LOG_INFO("Conga routing restarts dre event scheduling, Switch:" << m_switch_id << now);
			m_dreEvent = Simulator::Schedule(m_dreTime, &SwitchNode::DreEvent, this);
		}

		// Turn on aging event scheduler if it is not running
		if (!m_agingEvent.IsRunning())
		{
			NS_LOG_INFO("Conga routing restarts aging event scheduling:" << m_switch_id << now);
			m_agingEvent = Simulator::Schedule(m_agingTime, &SwitchNode::AgingEvent, this);
		}

		// get srcToRId, dstToRId
		assert(routeSettings::hostIp2SwitchId.find(Ipv4Address(ch.sip)) !=
			   routeSettings::hostIp2SwitchId.end()); // Misconfig of Settings::hostIp2SwitchId - sip"
		assert(routeSettings::hostIp2SwitchId.find(Ipv4Address(ch.dip)) !=
			   routeSettings::hostIp2SwitchId.end()); // Misconfig of Settings::hostIp2SwitchId - dip" //user.cc is not finished

		uint32_t srcToRId = routeSettings::hostIp2SwitchId[Ipv4Address(ch.sip)];

		uint32_t srcHostId = routeSettings::hostIp2IdMap[Ipv4Address(ch.sip)];

		uint32_t dstHostId = routeSettings::hostIp2IdMap[Ipv4Address(ch.dip)];
		uint32_t dstToRId = routeSettings::hostIp2SwitchId[Ipv4Address(ch.dip)];
		NS_LOG_INFO("srcHostId" << srcHostId << " dstHostId " << dstHostId);
		/** FILTER: Quickly filter intra-pod traffic */
		if (srcToRId == dstToRId)
		{ // do normal routing (only one path)
			auto it = m_congaIp2ports.find(Ipv4Address(ch.dip));
			CandidatePortEntry portEntries = m_congaIp2ports[Ipv4Address(ch.dip)];
			if (it == m_congaIp2ports.end())
			{
				std::cout << "Error in GetAllCandidateEgressPorts(): found No matched routing entries" << std::endl;
				selectedPort = 0;
			}
			else
			{
				selectedPort = portEntries.ports[rand() % portEntries.ports.size()];
				NS_LOG_INFO("NodeID:" << GetId() << " srcToRId == dstToRId,SELECT PORT:" << selectedPort);
			}
			return selectedPort;
		}

		// it should be not in the same pod
		assert(srcToRId != dstToRId && "Should not be in the same pod");

		// get QpKey to find flowlet
		// uint64_t qpkey = GetQpKey(ch.dip, ch.udp.sport, ch.udp.dport, ch.udp.pg);

		std::string flowid = GetStringHashValueFromCustomHeader(ch);

		// get CongaTag from packet
		std::ostringstream oss;
		bool found = p->PeekPacketTag(congaTag);
		if (!found)
		{ // sender-side
			if (!m_recordEvent.IsRunning())
			{
				NS_LOG_INFO("ConWeave routing restarts aging event scheduling:" << m_switch_id << now);
				m_recordEvent = Simulator::Schedule(m_recordTime, &SwitchNode::RecordPathload, this);
			}
			HostId2PathSeleKey reversePstKey(dstHostId, srcHostId);
			pstEntryData *reversePstEntry = routePath.lookup_PST(reversePstKey);
			uint32_t forwardPathNum = reversePstEntry->pathNum;
			uint32_t rndPathIdx = std::rand() % forwardPathNum;
			uint32_t rndPathId = reversePstEntry->paths[rndPathIdx];
			PathData *rndPiggyBackPitEntry = routePath.lookup_PIT(rndPathId);
			congaTag.SetHopCount(0);						 // hopCount
			congaTag.SetFbPathId(rndPiggyBackPitEntry->pid); // path
			congaTag.SetFbMetric(rndPiggyBackPitEntry->pathDre);

			/*---- choosing outPort ----*/
			struct CongaFlowlet *flowlet = NULL;
			auto flowletItr = m_congaflowletTable.find(flowid);
			uint32_t selectedPath;
			HostId2PathSeleKey forwarPstKey(srcHostId, dstHostId);
			// 1) when flowlet already exists
			if (flowletItr != m_congaflowletTable.end())
			{
				flowlet = flowletItr->second;
				assert(flowlet != NULL &&
					   "Impossible in normal cases - flowlet is not correctly registered");

				if (now - flowlet->activeTime <= m_flowletTimeout)
				{ // no timeout
					// update flowlet info
					flowlet->activeTime = now;
					flowlet->nPackets++;

					// update/measure CE of this outPort and add CongaTag
					selectedPath = flowlet->PathId;
					selectedPort = GetOutPortFromPath(selectedPath, 0); // sender switch is 0th hop
					uint32_t X = UpdateLocalDre(p, ch, selectedPort);	// update
					uint32_t localCe = QuantizingX(selectedPort, X);	// quantize
					congaTag.SetCe(localCe);
					congaTag.SetPathId(selectedPath);

					p->AddPacketTag(congaTag);
					oss << "SenderToR: " << m_switch_id << " Flowlet exists " << ""
						<< "Path/CE/outPort Path " << selectedPath << " CE "
						<< congaTag.GetCe() << " Port " << selectedPort << " FbPath/Metric FbPath "
						<< congaTag.GetFbPathId() << " Metric " << congaTag.GetFbMetric() << " Time "
						<< now;

					// DoSwitchSend(p, ch, GetOutPortFromPath(selectedPath, congaTag.GetHopCount()),ch.udp.pg);
					NS_LOG_INFO(oss.str());
					congaoutinfo[m_switch_id][now.GetMilliSeconds()] = oss.str();
					//  GetOutPortFromPath(selectedPath, congaTag.GetHopCount());
					// congaoutinfo.congatag = congaTag;
					// congaoutinfo.port = selectedPort;
					return selectedPort;
				}
				/*---- Flowlet Timeout ----*/
				// NS_LOG_INFO("Flowlet expires, calculate the new port");

				// selectedPath = GetBestPath(dstToRId, 4);
				selectedPath = GetCongaBestPath(forwarPstKey, 4);
				SwitchNode::nFlowletTimeout++;

				// update flowlet info
				flowlet->activatedTime = now;
				flowlet->activeTime = now;
				flowlet->nPackets++;
				flowlet->PathId = selectedPath;

				// update/add CongaTag
				selectedPort = GetOutPortFromPath(selectedPath, 0);
				uint32_t X = UpdateLocalDre(p, ch, selectedPort); // update
				uint32_t localCe = QuantizingX(selectedPort, X);  // quantize
				updatePathLoad(p->GetSize(), selectedPath);
				congaTag.SetCe(localCe);
				congaTag.SetPathId(selectedPath);
				congaTag.SetHopCount(0);

				p->AddPacketTag(congaTag);
				oss << "SenderToR " << m_switch_id << " Flowlet exists & Timeout" << " "
					<< "Path/CE/outPort Path " << selectedPath << " CE "
					<< congaTag.GetCe() << " Port " << selectedPort << " FbPath/Metric FbPath "
					<< congaTag.GetFbPathId() << " Metric " << congaTag.GetFbMetric() << " Time " << now;
				NS_LOG_INFO(oss.str());
				congaoutinfo[m_switch_id][now.GetMilliSeconds()] = oss.str();
				return selectedPort;
			}

			// 2) flowlet does not exist, e.g., first packet of flow
			selectedPath = GetCongaBestPath(forwarPstKey, 4);

			struct CongaFlowlet *newFlowlet = new CongaFlowlet;
			newFlowlet->activeTime = now;
			newFlowlet->activatedTime = now;
			newFlowlet->nPackets = 1;
			newFlowlet->PathId = selectedPath;
			m_congaflowletTable[flowid] = newFlowlet;

			// update/add CongaTag
			selectedPort = GetOutPortFromPath(selectedPath, 0);
			uint32_t X = UpdateLocalDre(p, ch, selectedPort); // update
			uint32_t localCe = QuantizingX(selectedPort, X);  // quantize
			congaTag.SetCe(localCe);
			congaTag.SetPathId(selectedPath);

			p->AddPacketTag(congaTag);
			oss << "SenderToR" << m_switch_id << " Flowlet does not exist " << "/n"
				<< " Path/CE/outPort Path " << selectedPath << " CE "
				<< congaTag.GetCe() << " Port " << selectedPort << " FbPath/Metric FbPath "
				<< congaTag.GetFbPathId() << " Metric " << congaTag.GetFbMetric() << " Time " << now;
			NS_LOG_INFO(oss.str());
			congaoutinfo[m_switch_id][now.GetMilliSeconds()] = oss.str();

			return selectedPort;
		}
		else if (reach_the_last_hop_of_path_tag(congaTag) == true)
		{ // receiver-side

			/*---- receiver-side ----*/
			// update CongaToLeaf table
			// auto toLeafItr = m_congaToLeafTable.find(srcToRId);

			// assert(toLeafItr != m_congaToLeafTable.end() && "Cannot find srcToRId from ToLeafTable");
			// auto innerToLeafItr = (toLeafItr->second).find(congaTag.GetFbPathId());

			HostId2PathSeleKey reversePstKey(dstHostId, srcHostId);
			// pstEntryData *reversePstEntry = routePath.lookup_PST(reversePstKey);
			//  uint32_t forwardPathNum = reversePstEntry->pathNum;
			//  PathData *rndPiggyBackPitEntry = routePath.lookup_PIT(rndPathId);

			if (congaTag.GetFbPathId() != CONGA_NULL &&
				congaTag.GetFbMetric() != CONGA_NULL)
			{ // if valid feedback
				PathData *feedBackPitEntry = routePath.lookup_PIT(congaTag.GetFbPathId());
				feedBackPitEntry->pathDre = congaTag.GetFbMetric();
				feedBackPitEntry->updateTime = now;
			}

			PathData *curPitEntry = routePath.lookup_PIT(congaTag.GetPathId());
			curPitEntry->pathDre = congaTag.GetCe();
			curPitEntry->updateTime = now;
			uint32_t hopCount = congaTag.GetHopCount() + 1;
			selectedPort = GetOutPortFromPath(congaTag.GetPathId(), hopCount);
			// remove congaTag from header
			p->RemovePacketTag(congaTag);
			oss << "ReceiverToR " << m_switch_id << " " << " Path/CE Path " << congaTag.GetPathId()
				<< " CE" << congaTag.GetCe() << " FbPath/Metric FbPath "
				<< congaTag.GetFbPathId() << " Metric " << congaTag.GetFbMetric() << " Time " << now;

			NS_LOG_INFO(oss.str());
			congaoutinfo[m_switch_id][now.GetMilliSeconds()] = oss.str();
			return selectedPort;
		}
		else
		{
			// mid switch
			// extract CongaTag
			assert(found && "If not ToR (leaf), CongaTag should be found");
			// get/update hopCount
			uint32_t hopCount = congaTag.GetHopCount() + 1;
			congaTag.SetHopCount(hopCount);

			// get outPort
			selectedPort = GetOutPortFromPath(congaTag.GetPathId(), hopCount); // getout?
			uint32_t X = UpdateLocalDre(p, ch, selectedPort);				   // update
			uint32_t localCe = QuantizingX(selectedPort, X);				   // quantize
			uint32_t congestedCe = std::max(localCe, congaTag.GetCe());		   // get more congested link's CE
			congaTag.SetCe(congestedCe);									   // update CE

			// Re-serialize congaTag
			CongaTag temp_tag;
			p->RemovePacketTag(temp_tag);
			p->AddPacketTag(congaTag);
			oss << "Agg/CoreSw" << m_switch_id << " " << " CE/outPort " << "localCe " << localCe << " CE "
				<< congaTag.GetCe() << " Port " << selectedPort << " FbPath/Metric FbPath "
				<< congaTag.GetFbPathId() << " Metric " << congaTag.GetFbMetric() << " Time " << now;

			NS_LOG_INFO(oss.str());
			congaoutinfo[m_switch_id][now.GetMilliSeconds()] = oss.str();
			return selectedPort;
		}
		assert(false && "not arrive here,CongaTag should be found or not be found");
		return 0;
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
			leastLoad = itr->second;
		}
		uint32_t sampleNum = m_d < allPorts.size() ? m_d : allPorts.size();

		for (uint32_t samplePortidx = 0; samplePortidx < sampleNum; samplePortidx++)
		{
			uint32_t sampleLoad = CalculateQueueLength(allPorts[samplePortidx]);
			if (sampleLoad < leastLoad)
			{
				leastLoad = sampleLoad;
				leastLoadInterface = allPorts[samplePortidx];
			}
			else if (allPorts[samplePortidx] == leastLoadInterface)
			{
				leastLoad = sampleLoad;
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
		letflowSaveEntry RecordEntry;
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
				RecordEntry.timeGap = (now - flowlet.activeTime).GetMicroSeconds();
				RecordEntry.currPort = flowlet.port;
				RecordEntry.lastPort = flowlet.port;
				RecordEntry.activeTime = flowlet.activeTime.GetMicroSeconds();
				flowlet.activeTime = now;

				// Return the port information used for routing routine to select the port
				selectedPort = flowlet.port;

				m_flowletTable[flowId] = flowlet;
				m_letflowTestInf[GetId()][flowId][now.GetMicroSeconds()] = RecordEntry;

				return selectedPort;
			}
			else
			{
				NS_LOG_INFO(GetId() << " not hit flowletTable ,Random select the port" << ", since timegap is: " << (now - flowletItr->second.activeTime).GetNanoSeconds() << "ns");
				RecordEntry.timeGap = (now - flowlet.activeTime).GetMicroSeconds();
				RecordEntry.lastPort = flowlet.port;
				RecordEntry.activeTime = flowlet.activeTime.GetMicroSeconds();
			}
		}

		// Not hit. Random Select the Port
		selectedPort = routeEntries.ports[rand() % routeEntries.ports.size()];

		LetFlowFlowletInfo flowlet;

		flowlet.port = selectedPort;
		flowlet.activeTime = now;
		m_flowletTable[flowId] = flowlet;

		RecordEntry.currPort = selectedPort;
		m_letflowTestInf[GetId()][flowId][now.GetMicroSeconds()] = RecordEntry;

		NS_LOG_INFO(" Random select the port is" << selectedPort << std::endl);
		return selectedPort;
	}
	int SwitchNode::GetOutDev(Ptr<Packet> p, CustomHeader &ch)
	{
		uint32_t swnodeid = GetId();
		switch (m_lbSolution)
		{
		case LB_Solution::LB_ECMP:
		{
			NS_LOG_INFO("swNodeId " << swnodeid << " Apply Load Balancing Algorithm: " << "ECMP");
			uint32_t flowId = GetHashValueFromCustomHeader(ch);
			CandidatePortEntry ecmpEntry = GetEcmpRouteEntry(Ipv4Address(ch.dip));
			uint32_t egressPort = ecmpEntry.ports[flowId % ecmpEntry.ports.size()];
			std::string ipInfo = ipv4Address2string(Ipv4Address(ch.sip)) + " " + ipv4Address2string(Ipv4Address(ch.dip)) + " " + std::to_string(ch.udp.sport);
			NS_LOG_INFO(" flowId " << flowId << " " << ipInfo);
			if (m_ecmpPortInf[swnodeid].find(flowId) != m_ecmpPortInf[swnodeid].end())
			{
				m_ecmpPortInf[swnodeid][flowId][egressPort] += 1;
			}
			else
			{
				m_ecmpPortInf[swnodeid][flowId][egressPort] = 1;
			}
			return egressPort;
		}
		case LB_Solution::LB_PLB:
		{
			NS_LOG_INFO("Apply Load Balancing Algorithm: " << "PLB-ECMP");
			PlbRehashTag plbTag;
			if (!p->PeekPacketTag(plbTag))
			{
				NS_LOG_INFO("plbTag Tag does not exit");
			}
			uint32_t randNum = plbTag.GetRandomNum();
			uint32_t flowId = GetHashValueFromCustomHeader(ch);
			CandidatePortEntry ecmpEntry = GetEcmpRouteEntry(Ipv4Address(ch.dip));
			uint32_t egressPort = ecmpEntry.ports[(flowId + randNum) % ecmpEntry.ports.size()];
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
			// std::map<uint64_t, letflowSaveEntry> saveEntry;
			// m_letflowTestInf[GetId()][flowId] = saveEntry;
			uint32_t egressPort = GetLetFlowEgressPort(Ipv4Address(ch.dip), flowId);
			return egressPort;
		}
		case LB_Solution::LB_CONGA:
		{
			NS_LOG_INFO("Apply Load Balancing Algorithm: " << "CONGA");
			uint32_t egressPort = GetCongaEgressPort(p, ch);
			return egressPort;
		}
		case LB_Solution::LB_LAPS:
		{
			// Do ecmp
			return GetNormalEcmpPort(p, ch);
		}
		case LB_Solution::LB_E2ELAPS:
		{
			// Do ecmp
			return GetNormalEcmpPort(p, ch);
		}
		case LB_Solution::LB_CONWEAVE:
		{
			// Do ecmp to dvice
			NS_LOG_INFO("****LB_CONWEAVE GetNormalEcmpPort*******");
			return GetNormalEcmpPort(p, ch);
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
	uint32_t SwitchNode::GetNormalEcmpPort(Ptr<Packet> p, CustomHeader &ch)
	{
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
		if (!device->IsPfcEnabled())
		{

			std::cout << "nodeId" << GetId() << "PFC not enable" << std::endl;
			exit(1);
		}

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
		if (m_lbSolution == LB_Solution::LB_LAPS)
		{
			NS_LOG_INFO("Apply Competitor Load Balancing Algorithm:LAPS");
			m_mmu->m_SmartFlowRouting->RouteInput(p, ch);
		}
		else if (m_lbSolution == LB_Solution::LB_E2ELAPS)
		{
			m_mmu->m_SmartFlowRouting->RouteInput(p, ch);
		}
		else if (m_lbSolution == LB_Solution::LB_CONWEAVE)
		{
			m_mmu->m_ConWeaveRouting->RouteInput(p, ch);
		}else
		{
			SendToDevContinue(p, ch);
		}
	}

	void SwitchNode::SendToDevContinue(Ptr<Packet> p, CustomHeader &ch)
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
					// Ipv4SmartFlowPathTag pathTag;
					// p->PeekPacketTag(pathTag);
					// std::cout << "Drop packet on path " << " with seq " << ch.udp.seq << std::endl;
					// std::cout << "due to admission control on switch " << GetId() << std::endl;
					return; // Drop
				}
				CheckAndSendPfc(inDev, qIndex);
			}
			m_bytes[inDev][idx][qIndex] += p->GetSize();
			m_PortInf[GetId()][idx].Packetsize += p->GetSize();
			m_PortInf[GetId()][idx].Packetcount += 1;
			dev->SwitchSend(qIndex, p, ch);
		}
		else
			return; // Drop
	}

	void SwitchNode::DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev, uint32_t qIndex)
	{
		int idx = outDev;

		if (idx >= 0)
		{

			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(GetDevice(idx));

			NS_ASSERT_MSG(dev->IsLinkUp(), "The routing table look up should return link that is up");

			// determine the qIndex
			// uint32_t qIndex;
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
					Ipv4SmartFlowPathTag pathTag;
					if(p->PeekPacketTag(pathTag))
					{
						std::cout << "Drop packet on path " << pathTag.get_path_id() << " with seq " << ch.udp.seq << " ";
						std::cout << "due to admission control on switch " << GetId() << std::endl;
					}

					return; // Drop
				}
				CheckAndSendPfc(inDev, qIndex);
			}
			
			m_bytes[inDev][idx][qIndex] += p->GetSize();
			m_PortInf[GetId()][idx].Packetsize += p->GetSize();
			m_PortInf[GetId()][idx].Packetcount += 1;
			dev->SwitchSend(qIndex, p, ch);
		}
		else
			return; // Drop
	}

	/*void SwitchNode::SendToDev(Ptr<Packet> p, CustomHeader &ch)
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
	}*/

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
		if (m_lbSolution == LB_Solution::LB_CONGA)
		{
			m_congaIp2ports[dstAddr].ports.push_back(intf_idx);
		}
		if (m_lbSolution == LB_Solution::LB_PLB)
		{
			m_ecmpRouteTable[dstAddr].ports.push_back(intf_idx);
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
		
		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		ch.getInt = 1; // parse INT header
		p->PeekHeader(ch);		
		FlowIdTag t;
		NS_ASSERT_MSG(p != 0 && (p->PeekPacketTag(t) || ch.l3Prot == L3ProtType::PFC), "No FlowIdTag found in the packet");
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
