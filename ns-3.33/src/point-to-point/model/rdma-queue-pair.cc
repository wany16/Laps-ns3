#include <ns3/hash.h>
#include <ns3/uinteger.h>
#include <ns3/seq-ts-header.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include <ns3/simulator.h>
#include "ns3/ppp-header.h"
#include "rdma-queue-pair.h"
#include "ns3/common-user-model.h"

namespace ns3
{

	NS_LOG_COMPONENT_DEFINE("RdmaQueuePair");

	Time Irn::rtoTimeLow = MicroSeconds(100);
	Time Irn::rtoTimeHigh = MicroSeconds(320);
	uint32_t Irn::rtoPktNum = IRN_RE_TX_THRESHOLD;
	uint32_t Irn::reTxThresholdNPackets = IRN_OPTIMIZED_RE_TX_THRESHOLD_N_PACKETS;
	uint32_t Irn::reTxThresholdNNanoSeconds = IRN_OPTIMIZED_RE_TX_THRESHOLD_N_NANOSECONDS;
	uint64_t CcLaps::maxIncStage = 5;
	Irn::Mode Irn::mode = Irn::Mode::NONE;

	void Irn::SetMode(std::string mode)
	{
		if (mode == "IRN_OPT")
		{
			Irn::mode = Irn::Mode::IRN_OPT;
		}
		else if (mode == "NACK")
		{
			Irn::mode = Irn::Mode::NACK;
		}
		else if (mode == "NONE")
		{
			Irn::mode = Irn::Mode::NONE;
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid mode");
		}
	}

	std::string Irn::GetMode()
	{
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			return "IRN_OPT";
		}
		else if (Irn::mode == Irn::Mode::NACK)
		{
			return "NACK";
		}
		else if (Irn::mode == Irn::Mode::NONE)
		{
			return "NONE";
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid mode");
			return "";
		}
	}


	/**************************
	 * RdmaQueuePair
	 *************************/
	TypeId RdmaQueuePair::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::RdmaQueuePair")
														.SetParent<Object>();
		return tid;
	}


	RdmaQueuePair::RdmaQueuePair(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport)
	{
		startTime = Simulator::Now();
		sip = _sip;
		dip = _dip;
		sport = _sport;
		dport = _dport;
		m_size = 0;
		snd_nxt = snd_una = 0;
		m_pg = pg;
		m_ipid = 0;
		m_win = 0;
		m_baseRtt = 0;
		m_max_rate = 0;
		m_var_win = false;
		m_rate = 0;
		m_nextAvail = Time(0);
		mlx.m_alpha = 1;
		mlx.m_alpha_cnp_arrived = false;
		mlx.m_first_cnp = true;
		mlx.m_decrease_cnp_arrived = false;
		mlx.m_rpTimeStage = 0;
		hp.m_lastUpdateSeq = 0;
		for (uint32_t i = 0; i < sizeof(hp.keep) / sizeof(hp.keep[0]); i++)
			hp.keep[i] = 0;
		hp.m_incStage = 0;
		hp.m_lastGap = 0;
		hp.u = 1;
		for (uint32_t i = 0; i < IntHeader::maxHop; i++)
		{
			hp.hopState[i].u = 1;
			hp.hopState[i].incStage = 0;
		}

		tmly.m_lastUpdateSeq = 0;
		tmly.m_incStage = 0;
		tmly.lastRtt = 0;
		tmly.rttDiff = 0;

		dctcp.m_lastUpdateSeq = 0;
		dctcp.m_caState = 0;
		dctcp.m_highSeq = 0;
		dctcp.m_alpha = 1;
		dctcp.m_ecnCnt = 0;
		dctcp.m_batchSizeOfAlpha = 0;

		hpccPint.m_lastUpdateSeq = 0;
		hpccPint.m_incStage = 0;
	}

	void RdmaQueuePair::SetSize(uint64_t size)
	{
		m_size = size;
	}

	void RdmaQueuePair::SetWin(uint32_t win)
	{
		m_win = win;
	}

	void RdmaQueuePair::SetBaseRtt(uint64_t baseRtt)
	{
		m_baseRtt = baseRtt;
	}

	void RdmaQueuePair::SetVarWin(bool v)
	{
		m_var_win = v;
	}

void RdmaQueuePair::SetFlowId(int32_t v) {
    m_flow_id = v;
    m_irn.m_sack.socketId = v;

}
std::string RdmaQueuePair::GetStringHashValueFromQp()
{

	std::string stringhash = ipv4Address2string(sip) + "#" + ipv4Address2string(dip) + "#" + std::to_string(sport); // srcPort=dstPort
	return stringhash;
}

	void RdmaQueuePair::SetAppNotifyCallback(Callback<void> notifyAppFinish)
	{
		m_notifyAppFinish = notifyAppFinish;
	}

	uint64_t RdmaQueuePair::GetBytesLeft()
	{
		NS_LOG_FUNCTION(this << "m_flow_id" << m_flow_id);

		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			uint32_t sack_seq, sack_sz;
			if (m_irn.m_sack.peekFrontBlock(&sack_seq, &sack_sz)) 
			{
				if (snd_nxt == sack_seq) 
				{
					snd_nxt += sack_sz;
				}
			}
		}
		else if (Irn::mode == Irn::Mode::IRN)
		{
			uint32_t sack_seq, sack_sz;
			if (m_irn.m_sack.peekFrontBlock(&sack_seq, &sack_sz)) 
			{
				if (snd_nxt == sack_seq) 
				{
					snd_nxt += sack_sz;
					m_irn.m_sack.discardUpTo(snd_nxt);
				}
			}
		}
		else if (Irn::mode == Irn::Mode::NACK)
		{
			return GetBytesLeftForLaps();	
		}
		return m_size >= snd_nxt ? m_size - snd_nxt : 0;
	}

	uint64_t RdmaQueuePair::GetBytesLeftForLaps()
	{
		NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT || Irn::mode == Irn::Mode::NACK, "Called Only When Laps is enabled");
		if (Irn::mode == Irn::IRN_OPT)
		{
			uint32_t sack_seq, sack_sz;
			if (m_irn.m_sack.peekFrontBlock(&sack_seq, &sack_sz)) {
				if (snd_nxt == sack_seq) {
					snd_nxt += sack_sz;
				}
			}
			return m_size >= snd_nxt ? m_size - snd_nxt : 0;
		}
		else if (Irn::mode == Irn::NACK)
		{
			size_t lossySize = m_irn.m_sack.getLossyDataSize();
			size_t undSize = m_size >= snd_nxt ? m_size - snd_nxt : 0;
			return lossySize + undSize;
		}
		else
		{
			NS_ASSERT_MSG(false, "Unknown mode");
			exit(1);
		}
	}


	uint32_t RdmaQueuePair::GetHash(void)
	{
		union
		{
			struct
			{
				uint32_t sip, dip;
				uint16_t sport, dport;
			};
			char c[12];
		} buf;
		buf.sip = sip.Get();
		buf.dip = dip.Get();
		buf.sport = sport;
		buf.dport = dport;
		return Hash32(buf.c, 12);
	}

	void RdmaQueuePair::ResumeQueue()
	{
		NS_LOG_FUNCTION(this);
		NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT, "Called Only When TrnOptimized is enabled");
		// std::cout << "Time " << Simulator::Now().GetNanoSeconds() << " " << m_node_id << " Should exits recovery mode\n";
		m_irn.m_recovery = false;
		m_irn.m_last_recovery_time_in_ns = Simulator::Now().GetNanoSeconds();
		if (snd_una < m_irn.m_recovery_seq)
		{
			m_irn.m_dupAckCnt = 0;
		}
		snd_nxt = m_irn.m_max_next_seq > snd_nxt ? m_irn.m_max_next_seq : snd_nxt;
		return ;
	}

	void RdmaQueuePair::RecoverQueue()
	{
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			m_irn.m_recovery = true;
			m_irn.m_dupAckCnt = 0;
			m_irn.m_max_next_seq = snd_nxt;
			snd_nxt = snd_una;
			uint32_t firstSackSeq, firstSackLen;
			if (m_irn.m_sack.peekFrontBlock(&firstSackSeq, &firstSackLen))
			{
				m_irn.m_recovery_seq = firstSackSeq;
			}
			else
			{
				m_irn.m_recovery_seq = m_irn.m_max_next_seq;
			}
		}
		else{
			std::cout << "ERROR: RdmaQueuePair::RecoverQueue() is not implemented\n";
		}
			return ;
	}

	void RdmaQueuePair::RecoverQueueLaps()
	{
		NS_LOG_FUNCTION(this);
		NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT, "Called Only When TrnOptimized is enabled");
		// std::cout << "Time " << Simulator::Now().GetNanoSeconds() << " " << m_node_id << " enters recovery mode, ";
		// std::cout << "snd_nxt: " << snd_nxt << ", snd_una: " << snd_una << ", ";
			m_irn.m_recovery = true;
			m_irn.m_dupAckCnt = 0;
			m_irn.m_max_next_seq = m_irn.m_max_next_seq < snd_nxt ? snd_nxt : m_irn.m_max_next_seq;
			snd_nxt = snd_una;
			uint32_t firstSackSeq, firstSackLen;
			if (m_irn.m_sack.peekFrontBlock(&firstSackSeq, &firstSackLen))
			{
				m_irn.m_recovery_seq = firstSackSeq;
				// std::cout << "Recovery seq: " << m_irn.m_recovery_seq << std::endl;
			}
			else
			{
				NS_ASSERT_MSG(false, m_node_id << " has No SACK block to recover");
				exit(1);
			}
	}


	void RdmaQueuePair::RecoverQueueUponTimeout()
	{
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			m_irn.m_recovery = true;
			m_irn.m_dupAckCnt = 0;
			m_irn.m_max_next_seq = m_irn.m_max_next_seq < snd_nxt ? snd_nxt : m_irn.m_max_next_seq;
			snd_nxt = snd_una;
			m_irn.m_recovery_seq = m_irn.m_max_next_seq;
		}
		else{
			std::cerr << "ERROR: RdmaQueuePair::RecoverQueueUponTimeout() is not implemented\n";
		}
	}

	void RdmaQueuePair::Acknowledge(uint64_t ack)
	{

		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			if (ack > snd_una)
			{
				snd_una = ack;
				m_irn.m_dupAckCnt = 0;
			}
			else if (ack == snd_una)
			{
					m_irn.m_dupAckCnt += 1;
			}
			uint32_t firstSackSeq, firstSackLen;
			if (m_irn.m_sack.peekFrontBlock(&firstSackSeq, &firstSackLen))
			{
				if (snd_una == firstSackSeq && firstSackLen!=0)
					{
						snd_una += firstSackLen;
						m_irn.m_dupAckCnt = 0;	
					}
			}
			m_irn.m_sack.discardUpTo(snd_una);
			m_irn.m_highest_ack = snd_una > m_irn.m_highest_ack ? snd_una : m_irn.m_highest_ack;
			if (snd_una > snd_nxt) { snd_nxt = snd_una;	}

			if (m_irn.m_recovery)
			{
				if (m_irn.m_sack.peekFrontBlock(&firstSackSeq, &firstSackLen))
				{
					if (snd_nxt == firstSackSeq)
						{
							snd_nxt += firstSackLen;
						}
				}
				if(snd_nxt >= m_irn.m_recovery_seq)
				{
					ResumeQueue();
				}
			}else
			{
				if (m_irn.m_dupAckCnt >= Irn::reTxThresholdNPackets)
				{
					if ((m_irn.m_recovery_seq > snd_una)&&(Simulator::Now().GetNanoSeconds() - m_irn.m_last_recovery_time_in_ns < m_baseRtt))
					{
					}
					else
					{
						RecoverQueue();
					}
				}
			}
			return ;
		}
		else if (ack > snd_una)
		{
			snd_una = ack;
		}

	}

	bool IrnSackManager::checkNackedBlockAndUpdateSndNxt(uint64_t &snd_nxt){
		NS_LOG_FUNCTION(this << "snd_nxt" << snd_nxt);
    // query if block exists inside SACK table
    NS_LOG_INFO ("ExistingBlocks=" << *this);
    auto it = m_data.begin();
    for (; it != m_data.end(); ++it) {
        NS_LOG_INFO ("curBlock=[" << it->first << ", " << it->first + it->second << ")");
				NS_ASSERT_MSG(it->second != 0, "Block size should be positive");
        if (it->first <= snd_nxt && snd_nxt <= it->first + it->second) {
						snd_nxt = it->first + it->second;
            return true;
        }     
    }
    return false;
	}

	bool IrnSackManager::checkFirstNackedBlockAndUpdateSndUna(uint64_t &snd_una){
		NS_LOG_FUNCTION(this << "snd_una" << snd_una);
		uint32_t firstSackSeq, firstSackLen;
		if (peekFrontBlock(&firstSackSeq, &firstSackLen))
		{
			if (snd_una == firstSackSeq && firstSackLen != 0)
				{
					snd_una += firstSackLen;
					NS_LOG_INFO ("Sender advances snd_una from " << snd_una-firstSackLen << " to " << snd_una);
					return true;
				}
		}
		return false;

	}

	bool IrnSackManager::checkOutstandingDataAndUpdateLossyData(uint32_t pid, uint32_t seq){
		NS_LOG_FUNCTION(this);
		NS_LOG_INFO("AckTag with pid=" << pid << ", seq=" << seq);
		NS_LOG_INFO ("ExistingOutstandingData: ");
		for (auto it = m_outstanding_data.begin(); it != m_outstanding_data.end(); ++it)
		{
			NS_LOG_INFO ("pid=" << it->first << ", seq= " << ListToString(it->second));
		}
		
		auto it = m_outstanding_data.find(pid);
		if (it == m_outstanding_data.end())
		{
			NS_ASSERT_MSG(false, "Invalid pid");
			return false;
		}
		NS_ASSERT_MSG(it->second.size() > 0, "Invalid outstanding data");
		bool valid = false;
		bool lossy = false;
		auto it2 = it->second.begin();
		while (it2 != it->second.end())
		{
			if (it2->first != seq)
			{
				m_lossy_data.emplace_back(it2->first, it2->second);
				NS_LOG_INFO ("LossyData: pid=" << pid << ", seq=[" << it2->first << ", " << it2->second << ")");
				it2 = it->second.erase(it2);
				lossy = true;
			}
			else
			{
				it2 = it->second.erase(it2);
				valid = true;
				break;
			}
		}
		NS_LOG_INFO ("FinalOutstandingData: ");
		for (auto it = m_outstanding_data.begin(); it != m_outstanding_data.end(); ++it)
		{
			NS_LOG_INFO ("pid=" << it->first << ", seq= " << ListToString(it->second));
		}

		NS_ASSERT_MSG(valid, "Invalid seq");
		return lossy;
	}

void IrnSackManager::handleRto(uint32_t pid)
{
	NS_LOG_FUNCTION(this << "pid=" << pid);
	auto it = m_outstanding_data.find(pid);
	NS_ASSERT_MSG(it != m_outstanding_data.end(), "Invalid pid when handling RTO");
	NS_ASSERT_MSG(it->second.size() > 0, "Invalid outstanding data when handling RTO for path " << pid);
	auto it2 = it->second.begin();
	while (it2 != it->second.end())
	{
		m_lossy_data.emplace_back(it2->first, it2->second);
		it2 = it->second.erase(it2);
	}
}


	void RdmaQueuePair::AcknowledgeForLaps(uint64_t ack, uint32_t nackSeq, uint16_t nackSize, uint32_t fpid)
	{
		NS_LOG_FUNCTION(this << ack);
    NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT || Irn::mode == Irn::Mode::NACK, "Called Only When Laps is enabled");

		if (Irn::mode == Irn::Mode::NACK)
		{
			NS_ASSERT_MSG(nackSize != 0, "Invalid nackSize");
			NS_ASSERT_MSG(nackSeq >= snd_una, "Invalid nackSeq");
			m_irn.m_sack.sack(nackSeq, nackSize);
			m_irn.m_sack.checkFirstNackedBlockAndUpdateSndUna(snd_una);
			m_irn.m_sack.discardUpTo(snd_una);
			m_irn.m_sack.checkOutstandingDataAndUpdateLossyData(fpid, nackSeq);
			auto it = m_irn.m_sack.m_outstanding_data.find(fpid);
			NS_ASSERT_MSG( it != m_irn.m_sack.m_outstanding_data.end(), "Invalid fpid");
			if (it->second.size() > 0)
			{
				Time rto =  m_cb_getRtoTimeForPath(fpid);
				m_rtoSetCb(this, fpid, rto);
			}
			else
			{
				m_cb_cancelRtoForPath(this, fpid);
			}
			
		}
		else if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			if (nackSize != 0)
			{
				m_irn.m_sack.sack(nackSeq, nackSize);
			}
			if (ack > snd_una)
			{
				snd_una = ack;
				m_irn.m_dupAckCnt = 0;
				m_irn.m_sack.discardUpTo(snd_una);
				if(m_irn.m_sack.checkFirstNackedBlockAndUpdateSndUna(snd_una))
				{
					m_irn.m_sack.discardUpTo(snd_una);
				}
				m_irn.m_highest_ack = snd_una > m_irn.m_highest_ack ? snd_una : m_irn.m_highest_ack;
			}
			else if (ack == snd_una)
			{
				if (nackSize == 0)
				{
					m_irn.m_dupAckCnt = 0;
				}
				else
				{
					m_irn.m_dupAckCnt += 1;
				}
			}
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid mode");
		}
	}


std::pair<uint32_t, uint16_t> IrnSackManager::GetAndRemoveFirstLossyData() {
	NS_LOG_FUNCTION(this);
	NS_ASSERT_MSG(m_lossy_data.size(), "m_lossy_data is empty");
  std::pair<uint32_t, uint16_t> firstElement = m_lossy_data.front();
  m_lossy_data.pop_front();
  return firstElement;
}

void RdmaQueuePair::CheckAndUpdateQpStateForLaps()
{
	NS_LOG_FUNCTION(this);
	// NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT, "Called Only When Laps is enabled");
	if (Irn::mode == Irn::Mode::IRN_OPT)
	{
		snd_nxt = snd_una > snd_nxt ? snd_una : snd_nxt;
		m_irn.m_sack.checkNackedBlockAndUpdateSndNxt(snd_nxt);
		m_irn.m_max_next_seq = m_irn.m_max_next_seq < snd_nxt ? snd_nxt : m_irn.m_max_next_seq;

		if (m_irn.m_recovery && (snd_nxt >= m_irn.m_recovery_seq))
		{
				ResumeQueue();
		}
		else if (!m_irn.m_recovery && m_irn.m_dupAckCnt >= Irn::reTxThresholdNPackets)
		{
			bool isAlreadlyReTx = m_irn.m_recovery_seq > snd_una;
			bool isLastReTxExpired = Simulator::Now().GetNanoSeconds() - m_irn.m_last_recovery_time_in_ns > m_baseRtt;
			if (!isAlreadlyReTx || isLastReTxExpired)
			{
					RecoverQueueLaps();
			}
		}
	}
	else if (Irn::mode == Irn::Mode::NACK)
	{
		if (m_irn.m_sack.existLossyData())
		{
			std::pair<uint32_t, uint16_t> firstLossyPkt = m_irn.m_sack.GetAndRemoveFirstLossyData();
			NS_ASSERT_MSG(firstLossyPkt.second != 0 && firstLossyPkt.first >= snd_una, "Invalid lossy data");
			snd_nxt = firstLossyPkt.first;
		}
		else
		{
			snd_nxt = m_irn.m_max_next_seq;
		}
	}
	else
	{
		NS_ASSERT_MSG(false, "Invalid mode");
	}
}




	uint64_t RdmaQueuePair::GetOnTheFly()
	{
		NS_LOG_FUNCTION(this);
    NS_ASSERT(snd_nxt >= snd_una);
		if (Irn::mode == Irn::Mode::IRN_OPT){
			return GetOnTheFlyForLaps();
		}
		return snd_nxt - snd_una;
	}

	uint64_t RdmaQueuePair::GetOnTheFlyForLaps()
	{
		NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT || Irn::mode == Irn::Mode::NACK, "Called Only When Laps is enabled");
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			int64_t nacked = m_irn.m_sack.getSackBufferOverhead();
			int64_t onTheFly = m_irn.m_max_next_seq - snd_una - nacked;		
			NS_ASSERT_MSG(onTheFly >= 0, "onTheFly should be non-negative");
			return onTheFly;
		}
		else if (Irn::mode == Irn::Mode::NACK)
		{
			uint32_t onTheFly = m_irn.m_sack.getOutStandingDataSizeForLaps();
			NS_ASSERT_MSG(onTheFly >= 0, "onTheFly should be non-negative");
			return onTheFly;
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid mode");
			return 0;
		}

	}

	bool RdmaQueuePair::CanIrnTransmitForLaps(uint32_t mtu) {
		NS_LOG_FUNCTION(this << "MtuInByte " << mtu);
    NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT || Irn::mode == Irn::Mode::NACK, "Called Only When Laps is enabled");
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			uint64_t byteLeft = m_size >= snd_nxt ? m_size - snd_nxt : 0;
			uint64_t byteTx = byteLeft > mtu ? mtu : byteLeft;
			uint64_t byteOnFly = GetOnTheFlyForLaps();
			bool isBdpAllowed = (byteOnFly + byteTx) < GetWinForLaps() ? true : false;
			return isBdpAllowed;
		}
		else if (Irn::mode == Irn::Mode::NACK)
		{
			uint64_t byteLeft = m_size >= snd_nxt ? m_size - snd_nxt : 0;
			uint64_t byteTx = byteLeft > mtu ? mtu : byteLeft;
			uint64_t byteOnFly = GetOnTheFlyForLaps();
			bool isBdpAllowed = (byteOnFly + byteTx) <= GetWinForLaps() ? true : false;
			return isBdpAllowed;
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid mode");
			return false;
		}

	}


	bool RdmaQueuePair::CanIrnTransmit(uint32_t mtu) {
		NS_LOG_FUNCTION(this << "MtuInByte " << mtu);
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			return CanIrnTransmitForLaps(mtu);
		}

		NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN, "Called Only when Irn is enabled");
		uint64_t byteLeft = m_size >= snd_nxt ? m_size - snd_nxt : 0;
		uint64_t byteTx = byteLeft > mtu ? mtu : byteLeft;
		uint64_t byteOnFly = m_irn.GetOnTheFly();
		bool isBdpAllowed = (byteOnFly + byteTx) < m_irn.m_bdp ? true : false;
		if (isBdpAllowed) {
			bool isBdpExceeded = (m_irn.m_highest_ack + m_irn.m_bdp) > snd_nxt ? false : true;
			return !isBdpExceeded;
		}
		return false;
	
	}



	bool RdmaQueuePair::IsWinBound()
	{
		NS_LOG_FUNCTION(this << "enableVarWin" << m_var_win
												 << "maxWinInByte" << m_win
									 );
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			return IsWinBoundForLaps();
		}

		uint64_t w = GetWin();
		return w != 0 && GetOnTheFly() >= w;
	}

	bool RdmaQueuePair::IsWinBoundForLaps()
	{
		NS_LOG_FUNCTION(this);
		uint64_t w = GetWinForLaps();
		return w != 0 && GetOnTheFlyForLaps() >= w;
	}

	uint64_t RdmaQueuePair::GetWin()
	{
		NS_LOG_FUNCTION(this);
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			return GetWinForLaps();
		}

		if (m_win == 0)
		{
			return 0;
		}

		uint64_t w;
		if (m_var_win)
		{
			w = m_win * m_rate.GetBitRate() / m_max_rate.GetBitRate();
			if (w == 0)
			{
				w = 1; // must > 0
			}
		}
		else
		{
			w = m_win;
		}
		return w;
	}

	uint64_t RdmaQueuePair::GetWinForLaps()
	{
		NS_LOG_FUNCTION(this	<< "enableVarWin" << m_var_win
													<< "maxWinInByte" << m_win
									 );
		NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT || Irn::mode == Irn::Mode::NACK, "Called Only when Laps is enabled");

		if (m_win == 0)
		{
			return 0x7FFFFFFFFFFFFFFF;
		}

		uint64_t w;
		if (m_var_win)
		{
			w = m_win * laps.m_curRate.GetBitRate() / m_max_rate.GetBitRate();
			if (w == 0)
			{
				w = 1; // must > 0
			}
		}
		else
		{
			w = m_win;
		}
		return w;
	}

	uint64_t RdmaQueuePair::HpGetCurWin()
	{
		if (m_win == 0)
			return 0;
		uint64_t w;
		if (m_var_win)
		{
			w = m_win * hp.m_curRate.GetBitRate() / m_max_rate.GetBitRate();
			if (w == 0)
				w = 1; // must > 0
		}
		else
		{
			w = m_win;
		}
		return w;
	}

	bool RdmaQueuePair::IsFinished()
	{
		if (Irn::mode == Irn::Mode::IRN) {
			uint32_t sack_seq, sack_sz;
			if (m_irn.m_sack.peekFrontBlock(&sack_seq, &sack_sz)) {
				if (snd_nxt == sack_seq) {
						snd_nxt += sack_sz;
						m_irn.m_sack.discardUpTo(snd_nxt);
				}
			}
		}
		else if (Irn::mode == Irn::Mode::IRN_OPT) {
			m_irn.m_sack.checkFirstNackedBlockAndUpdateSndUna(snd_una);
			return snd_una >= m_size;
		}
		else if (Irn::mode == Irn::Mode::NACK)
		{
			m_irn.m_sack.checkFirstNackedBlockAndUpdateSndUna(snd_una);
			return snd_una >= m_size;
		}
		return snd_una >= m_size;
	}

	/*********************
	 * RdmaRxQueuePair
	 ********************/
	TypeId RdmaRxQueuePair::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::RdmaRxQueuePair")
														.SetParent<Object>();
		return tid;
	}

	RdmaRxQueuePair::RdmaRxQueuePair()
	{
		sip = dip = sport = dport = 0;
		m_ipid = 0;
		ReceiverNextExpectedSeq = 0;
		m_nackTimer = Time(0);
		m_milestone_rx = 0;
		m_lastNACK = 0;
		m_flow_id = -1;
	}

	uint32_t RdmaRxQueuePair::GetHash(void)
	{
		union
		{
			struct
			{
				uint32_t sip, dip;
				uint16_t sport, dport;
			};
			char c[12];
		} buf;
		buf.sip = sip;
		buf.dip = dip;
		buf.sport = sport;
		buf.dport = dport;
		return Hash32(buf.c, 12);
	}

	/*********************
	 * RdmaQueuePairGroup
	 ********************/
	TypeId RdmaQueuePairGroup::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::RdmaQueuePairGroup")
														.SetParent<Object>();
		return tid;
	}

	RdmaQueuePairGroup::RdmaQueuePairGroup(void)
	{
	}

	uint32_t RdmaQueuePairGroup::GetN(void)
	{
		return m_qps.size();
	}

	Ptr<RdmaQueuePair> RdmaQueuePairGroup::Get(uint32_t idx)
	{
		return m_qps[idx];
	}

	Ptr<RdmaQueuePair> RdmaQueuePairGroup::operator[](uint32_t idx)
	{
		return m_qps[idx];
	}

	void RdmaQueuePairGroup::AddQp(Ptr<RdmaQueuePair> qp)
	{
		m_qps.push_back(qp);
	}

#if 0
void RdmaQueuePairGroup::AddRxQp(Ptr<RdmaRxQueuePair> rxQp){
	m_rxQps.push_back(rxQp);
}
#endif

	void RdmaQueuePairGroup::Clear(void)
	{
		m_qps.clear();
	}

	IrnSackManager::IrnSackManager()
	{
		socketId = -1;
		m_data.clear();
	}
	IrnSackManager::IrnSackManager(int flow_id)
	{
		socketId = flow_id;
		m_data.clear();
	}

	std::ostream &operator<<(std::ostream &os, const IrnSackManager &im)
	{
		auto it = im.m_data.begin();
		bool isFirstSack = true;
		for (; it != im.m_data.end(); ++it)
		{
			uint32_t blockBegin = it->first;						// inclusive
			uint32_t blockEnd = it->first + it->second; // exclusive
			if (isFirstSack)
			{
				os << "[" << blockBegin << "," << blockEnd << ") ";
	      isFirstSack = false;
			}
			else
			{
				os << ", [" << blockBegin << "," << blockEnd << ")";
			} 
		}
		if (isFirstSack)
		{
			os << "()";
		}
			
		return os;
	}

void IrnSackManager::sack(uint32_t seq, uint32_t sz) {
    NS_LOG_FUNCTION(this << "fid=" << socketId << ", block= [" << seq << "," << seq+sz << ")");
    // NS_LOG_INFO("fid=" << socketId << " has " << m_data.size() << " blocks=" << *this);
    if (sz == 0) { 
        NS_LOG_LOGIC("Ignore empty block");
        return;
    }
    NS_LOG_INFO("try inserting the block=[" << seq << ", " << (seq + sz) << ") to " << *this);
    uint32_t seqEnd = seq + sz;  // exclusive
    auto it = m_data.begin();
    for (; it != m_data.end(); ++it) {
        uint32_t blockBegin = it->first;             // inclusive
        uint32_t blockEnd = it->first + it->second;  // exclusive
        // NS_LOG_INFO("traversingBlock=[" << blockBegin << ", " << blockEnd << ")");
        // NS_LOG_INFO("InsertingBlock=[" << seq << ", " << seqEnd << ")");
        if (blockBegin <= seq && seqEnd <= blockEnd) {
            // NS_LOG_LOGIC("InsertingBlock is *Contained* by the traversingBlock ==> Ignore");  // seq-seqEnd is included inside block-blockEnd
            return;
        } else if (seq < blockBegin && blockEnd < seqEnd) {
            // NS_LOG_LOGIC("InsertingBlock *Contains* the traversingBlock");
            m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, blockBegin - seq));
            // NS_LOG_INFO("Insert the Front-Isolated Block=[" << seq << ", " << blockBegin << ")");
            seq = blockEnd;
            sz = seqEnd - blockEnd;
            seqEnd = seq + sz;
            // NS_LOG_INFO("InsertingBlock changes to [" << seq << ", " << seqEnd << ")");
        } else if (seq < blockBegin && seqEnd <= blockBegin) {
            // NS_LOG_LOGIC("InsertingBlock is *Smaller* than the traversingBlock");
            m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, sz));
            // NS_LOG_INFO("Insert the Block=[" << seq << ", " << seq+sz << ")");
            sz = 0;
            // NS_LOG_INFO("Finish the Insertion Process");
            break;
        } else if (blockBegin <= seq && seq <= blockEnd && blockEnd < seqEnd) {
            // NS_LOG_LOGIC("InsertingBlock is *Front-Overlapped* with the traversingBlock");
            seq = blockEnd;
            sz = seqEnd - blockEnd;
            // NS_LOG_INFO("InsertingBlock changes to [" << seq << ", " << sz+seq << ")");
        } else if (seq < blockBegin && blockBegin <= seqEnd && seqEnd <= blockEnd) {
            // NS_LOG_LOGIC("InsertingBlock is *Tail-Overlapped* with the traversingBlock");
            m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, blockBegin - seq));
            // NS_LOG_INFO("Insert the Block=[" << seq << ", " << blockBegin - seq << ")");
            sz = 0;
            // NS_LOG_INFO("Finish the Insertion Process");
            break;
        } else {
            // NS_LOG_LOGIC("InsertingBlock is *Larger* than the traversingBlock");
            NS_ASSERT(blockEnd <= seq);
        }
    }
    if (sz != 0) {
        m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, sz));
        // NS_LOG_INFO("Append the block [" << seq << ", " << (seq + sz) << ") to the end");
    }
    NS_ASSERT_MSG(m_data.size() > 0, " Should have at least 1 Block");

    // Sanity check : check duplicate, empty blocks, merge neighboring blocks
    // NS_LOG_LOGIC("Sanity check: check duplicate and empty blocks, and merge neighboring blocks");
    // NS_LOG_INFO("There is " << m_data.size() << " blocks=" << *this);
    auto it_prev = m_data.begin();
    for (it = m_data.begin(); it != m_data.end();) {
        if (it == it_prev) {
            ++it;
            continue;
        }
        NS_ASSERT_MSG(it_prev->first + it_prev->second <= it->first, "Overlap or Contained or Containing Blocks");
        NS_ASSERT_MSG(it->second > 0, "Zero block length");
        if (it_prev->first + it_prev->second == it->first) {
            NS_LOG_INFO("Merging: [" << it_prev->first << ", " << it_prev->first + it_prev->second <<
																		") + ["<< it->first << ", " << (it->first + it->second) <<
																		") = [" << it_prev->first << ", " << (it_prev->first + it->second + it_prev->second) << ")");
            it_prev->second += it->second;
            it = m_data.erase(it);
        } else {
            it_prev = it;
            ++it;
        }
    }

    NS_LOG_INFO("Final Blocks= " << *this);
}





void IrnSackManager::appendOutstandingData (uint32_t pid, uint32_t seq, uint16_t size)
{
	NS_LOG_FUNCTION(this);
	NS_ASSERT_MSG(Irn::mode == Irn::Mode::NACK, "Called Only When NACK is enabled");
	auto it = m_outstanding_data.find(pid);
	// 如果 pid 不存在，则创建一个新的列表
	if (it == m_outstanding_data.end()) {
			m_outstanding_data[pid] = std::list<std::pair<uint32_t, uint16_t>>();
			it = m_outstanding_data.find(pid);
	}
	// 将数据添加到列表的末尾
	it->second.emplace_back(seq, size);
	NS_LOG_INFO( "Time: " << Simulator::Now().GetNanoSeconds() << ", OutStanding Data for pid " << pid << " is " << ListToString(it->second));
}



size_t IrnSackManager::discardUpTo(uint32_t cumAck) {
    NS_LOG_FUNCTION (this << cumAck);
    // NS_LOG_INFO ("Flow=" << socketId << ", " << "nextExpectedSeq=" << cumAck << ", Existingblocks= " << *this);
    auto it = m_data.begin(); 
    size_t erase_len = 0; // 初始化erase_len，用于记录被移除的数据包长度
    for (; it != m_data.end();) {
        // NS_LOG_INFO("TraversingBlock=[" << it->first <<", " << it->first + it->second << ").");
        if (it->first + it->second <= cumAck) {         // 如果已收到报文的结束序号小于等于期望收到的序列号，则从sack中移除此已收到的报文段
            // NS_LOG_LOGIC("Remove the entire TraversingBlock.");
            erase_len += it->second;
            // NS_LOG_INFO("RemovedLength=" << it->second);
						NS_LOG_INFO("Remove the entire block [" << it->first << ", " << it->first + it->second << ")");
            it = m_data.erase(it);
        }else if (it->first < cumAck) {         // 如果数据包的开始序号小于cumAck，则移除数据包的一部分
            // NS_LOG_LOGIC("Remove the *part* of TraversingBlock.");
            erase_len += cumAck - it->first;
						NS_LOG_INFO("Remove the part of block [" << it->first << ", " << cumAck << ")");
            // NS_LOG_INFO("RemovedLength=" << cumAck - it->first);
            it->second = it->first + it->second - cumAck; // second是len，first是起始点，求解新的len
            it->first = cumAck; //求解新的起始点
            NS_ASSERT(it->second != 0);
            // NS_LOG_LOGIC("Ignore the subsequent block.");
            break;
        }else {         // 如果首个数据包的开始序号大于cumAck，则跳出循环，无需合并数据包
            // NS_LOG_LOGIC("Maintain the block and Ignore the subsequent block.");
            break;
        }
    }
    // 返回被移除的数据包长度		
    // NS_LOG_INFO ("Existingblocks= " << *this);
    return erase_len;
}

bool IrnSackManager::IsEmpty()
	{
		NS_LOG_FUNCTION(this);
		if (Irn::mode == Irn::Mode::IRN_OPT)
		{
			return !m_data.size();
		}
		else if (Irn::mode == Irn::Mode::NACK)
		{
			return !m_lossy_data.size();
		}
		else if (Irn::mode == Irn::Mode::IRN)
		{
			return !m_data.size();
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid Irn Mode");
			return true;
		}
	}

bool IrnSackManager::existLossyData()
	{
		NS_LOG_FUNCTION(this);
		if (Irn::mode == Irn::Mode::NACK)
		{
			if (m_lossy_data.size() > 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			NS_ASSERT_MSG(false, "Invalid Irn Mode");
			return true;
		}
	}


bool IrnSackManager::blockExists(uint32_t seq, uint32_t size) {
    // query if block exists inside SACK table
    NS_LOG_FUNCTION (this << "targetBlock= [" << seq << ", " << seq + size << ")");
    NS_LOG_INFO ("ExistingBlocks=" << *this);
    auto it = m_data.begin();
    for (; it != m_data.end(); ++it) {
        NS_LOG_INFO ("TraversingBlock=[" << it->first << ", " << it->first + it->second << ")");
        if (it->first <= seq && seq + size <= it->first + it->second) {
            NS_LOG_LOGIC ("targetBlock is *entirely contained* by TraversingBlock");
            return true;
        }     
    }
    NS_LOG_LOGIC ("targetBlock is *NOT entirely contained* by Existing Blocks");
    return false;
}

bool IrnSackManager::peekFrontBlock(uint32_t* pseq, uint32_t* psize) {
    NS_LOG_FUNCTION (this);
    NS_ASSERT(pseq);
    NS_ASSERT(psize);
    if (!m_data.size()) {
        NS_LOG_LOGIC ("No block in Sack");
        *pseq = 0;
        *psize = 0;
        return false;
    }
    auto it = m_data.begin();
    *pseq = it->first;
    *psize = it->second;
    // NS_LOG_INFO ("FrontBlock=[" << it->first << ", " << it->first + it->second << ")");
    return true;
}

size_t IrnSackManager::getSackBufferOverhead() {
    NS_LOG_FUNCTION (this);
    size_t overhead = 0;
    auto it = m_data.begin();
    for (; it != m_data.end(); ++it) {
        overhead += it->second;  // Bytes
    }
    return overhead;
}

size_t IrnSackManager::getOutStandingDataSizeForLaps() {
    NS_LOG_FUNCTION (this);
    NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT || Irn::mode == Irn::Mode::NACK, "Called Only When Laps is enabled");
    size_t overhead = 0;
    auto it = m_outstanding_data.begin();
    for (; it != m_outstanding_data.end(); ++it) {
			for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			{
        overhead += it2->second;  // Bytes
			}
    }
    return overhead;
}

size_t IrnSackManager::getLossyDataSize() {
    NS_LOG_FUNCTION (this);
    NS_ASSERT_MSG(Irn::mode == Irn::Mode::NACK, "Called Only When Laps is enabled");
    size_t s = 0;
		for (auto it = m_lossy_data.begin(); it != m_lossy_data.end(); ++it) {
				s += it->second;
		}
    return s;
}

}
