#ifndef RDMA_QUEUE_PAIR_H
#define RDMA_QUEUE_PAIR_H

#include <ns3/object.h>
#include <ns3/packet.h>
#include <ns3/ipv4-address.h>
#include <ns3/data-rate.h>
#include <ns3/event-id.h>
#include <ns3/custom-header.h>
#include <ns3/int-header.h>
#include <vector>
#include <climits> /* for CHAR_BIT */
#include <ns3/event-id.h>
#include "ns3/traced-callback.h"
#include <random>

#define MAX_RDMA_FLOW_PER_HOST 9120
#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)
#define IRN_RE_TX_THRESHOLD 3
#define IRN_OPTIMIZED_RE_TX_THRESHOLD_N_PACKETS 2
#define IRN_OPTIMIZED_RE_TX_THRESHOLD_N_NANOSECONDS 1000000



namespace ns3 {

enum CcMode {
    CC_MODE_DCQCN = 1,
    CC_MODE_HPCC = 3,
    CC_MODE_TIMELY = 7,
    CC_MODE_DCTCP = 8,
    CC_MODE_UNDEFINED = 0,
};





struct CcLaps {
	DataRate m_tgtRate;	//< Target rate
	DataRate m_curRate;	//< Target rate
	uint32_t m_incStage{0};	//< Target delay
	int64_t m_tgtDelayInNs{-1};	//< Target delay
	uint64_t m_nxtRateDecTimeInNs{0};	//< Next time to decrease rate
	uint64_t m_nxtRateIncTimeInNs{0};	//< Next time to increase rate
	static uint64_t maxIncStage;
};


class IrnSackManager {
   private:
    std::list<std::pair<uint32_t, uint32_t>> m_data;

   public:
    int socketId{-1};
		std::map <uint32_t, std::list<std::pair<uint32_t, uint16_t> > > m_outstanding_data;
		std::list <std::pair<uint32_t, uint16_t> > m_lossy_data;
		void handleRto(uint32_t pid);
    IrnSackManager();
    IrnSackManager(int flow_id);
    void sack(uint32_t seq, uint32_t size);  // put blocks
    size_t discardUpTo(uint32_t seq);        // return number of blocks removed
    bool IsEmpty();
    bool blockExists(uint32_t seq, uint32_t size);  // query if block exists inside SACK table
    bool peekFrontBlock(uint32_t *pseq, uint32_t *psize);
		std::pair<uint32_t, uint16_t> GetAndRemoveFirstLossyData();
		bool checkNackedBlockAndUpdateSndNxt(uint64_t &snd_nxt);
		bool checkFirstNackedBlockAndUpdateSndUna(uint64_t &snd_una);
		bool existLossyData();
    size_t getSackBufferOverhead();  // get buffer overhead
		void appendOutstandingData (uint32_t pid, uint32_t seq, uint16_t size);
		bool checkOutstandingDataAndUpdateLossyData(uint32_t pid, uint32_t seq);
		size_t getOutStandingDataSizeForLaps();
		size_t getLossyDataSize();
		size_t getFirstLossyDataSize();
    friend std::ostream &operator<<(std::ostream &os, const IrnSackManager &im);
};

struct Irn{
		enum Mode
		{
			GBN = 0,
			IRN = 1,
			IRN_OPT = 2,
			NACK= 3,
			NONE = 4
		};
		// bool m_enabled;
		uint32_t m_bdp;          // m_irn_maxAck_
		uint32_t m_highest_ack{0};  // the so-far max acked-seq, before/during/after entering the recovery mode
		uint32_t m_max_seq;      // the so-far max sent-seq, before/during/after entering the recovery mode
		static Time rtoTimeLow;
		static Time rtoTimeHigh;
		static uint32_t rtoPktNum;
		static bool isIrnEnabled;
		static bool isTrnOptimizedEnabled;
		static uint32_t reTxThresholdNPackets;
		static uint32_t reTxThresholdNNanoSeconds;
		static Irn::Mode mode;
		static void SetMode(std::string mode);
		static std::string GetMode();
		static bool isWindowBasedForLaps;

		EventId m_reTxEvent;
		uint32_t m_lastReTxSeq{0};
		uint32_t m_nextReTxSeq{0};		
		uint32_t m_dupAckCnt{0};
		uint32_t m_max_next_seq{0}; 
		uint64_t m_last_recovery_time_in_ns{0}; 

		IrnSackManager m_sack;
		bool m_recovery{false};
		bool m_isTimeOut{false};
		uint32_t m_recovery_seq{0};// snd_nxt upon entering the recovery mode
		uint32_t GetOnTheFly() const {
				// IRN do not consider SACKed segments for simplicity, to be optimized.
				if (m_max_seq < m_highest_ack) {
					std::cerr << "m_max_seq: " << m_max_seq << " -> m_highest_ack: " << m_highest_ack << std::endl;
					return 0;
				}else{
					return m_max_seq - m_highest_ack;
				}
		}

		Time GetRto(uint32_t mtu) const {
			if (GetOnTheFly() > rtoPktNum * mtu) {
					return rtoTimeHigh;
			}
			return rtoTimeLow;
		}
};






/******************************
 * add from conweave, 2024/12/10 */

//****************************


class RdmaQueuePair : public Object {
public:
	Time startTime;
	Ipv4Address sip, dip;
	uint16_t sport, dport;
	uint64_t m_size;
	uint64_t snd_nxt, snd_una; // next seq to send, the highest unacked seq
	uint16_t m_pg;
	uint16_t m_ipid;
	uint32_t m_win; // bound of on-the-fly packets
	uint64_t m_baseRtt; // base RTT of this qp
	DataRate m_max_rate; // max rate
	bool m_var_win; // variable window size
	Time m_nextAvail;	//< Soonest time of next send
	uint32_t wp; // current window of packets
	uint32_t lastPktSize;
	Callback<void> m_notifyAppFinish;
	uint64_t sendDateSize = 0;
	int32_t m_flow_id; // conweave
	Time m_timeout;

	//bool enablePLBHash //PLB
    uint32_t flowRandNum=0; //PLB
	uint32_t m_node_id;
	Irn m_irn;
	// static bool isIrnEnabled;

	/******************************
	 * runtime states
	 *****************************/
	DataRate m_rate;	//< Current rate
	TracedCallback<Ptr<RdmaQueuePair>, DataRate, DataRate> m_rateTrace;
	void TraceRate(DataRate rate);

	struct {
		DataRate m_targetRate;	//< Target rate
		EventId m_eventUpdateAlpha;
		double m_alpha;
		bool m_alpha_cnp_arrived; // indicate if CNP arrived in the last slot
		bool m_first_cnp; // indicate if the current CNP is the first CNP
		EventId m_eventDecreaseRate;
		bool m_decrease_cnp_arrived; // indicate if CNP arrived in the last slot
		uint32_t m_rpTimeStage;
		EventId m_rpTimer;
	} mlx;
	struct {
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		IntHop hop[IntHeader::maxHop];
		uint32_t keep[IntHeader::maxHop];
		uint32_t m_incStage;
		double m_lastGap;
		double u;
		struct {
			double u;
			DataRate Rc;
			uint32_t incStage;
		}hopState[IntHeader::maxHop];
	} hp;
	struct{
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		uint32_t m_incStage;
		uint64_t lastRtt;
		double rttDiff;
	} tmly;
	struct{
		uint32_t m_lastUpdateSeq;
		uint32_t m_caState;
		uint32_t m_highSeq; // when to exit cwr
		double m_alpha;
		uint32_t m_ecnCnt;
		uint32_t m_batchSizeOfAlpha;
	} dctcp;
	struct{
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		uint32_t m_incStage;
	}hpccPint;
	
	CcLaps laps;


	Time GetRto(uint32_t mtu) const{
			if (Irn::mode == Irn::Mode::IRN) {
					return m_irn.GetRto(mtu);
			}
			else if (Irn::mode == Irn::Mode::IRN_OPT)
			{
				return m_irn.GetRto(mtu);
			}
			else {
					return m_timeout;
			}
	}

	bool CanIrnTransmit(uint32_t mtu);

	// inline bool CanIrnTransmitForLaps(uint32_t mtu) const {
	// 	NS_ASSERT_MSG(Irn::mode == Irn::Mode::IRN_OPT, "Only TrnOptimized can be enabled");
	// 	uint64_t byteLeft = m_size >= snd_nxt ? m_size - snd_nxt : 0;
	// 	uint64_t byteTx = byteLeft > mtu ? mtu : byteLeft;
	// 	uint64_t byteOnFly = GetOnTheFlyForLaps();
	// 	bool isBdpAllowed = (byteOnFly + byteTx) < GetWinForLaps() ? true : false;
	// 	return isBdpAllowed;
	// }


		uint64_t m_phyTxNPkts{0};
		uint64_t m_phyTxBytes{0};


    // Implement Timeout according to IB Spec Vol. 1 C9-139.
    // For an HCA requester using Reliable Connection service, to detect missing responses,
    // every Send queue is required to implement a Transport Timer to time outstanding requests.
    EventId m_retransmit;

	/***********
	 * methods
	 **********/
	static TypeId GetTypeId (void);
	RdmaQueuePair(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport);
	void SetRate(DataRate rate);
	void SetSize(uint64_t size);
	void SetWin(uint32_t win);
	void SetBaseRtt(uint64_t baseRtt);
	void SetVarWin(bool v);
	void SetAppNotifyCallback(Callback<void> notifyAppFinish);
  void SetFlowId(int32_t v);
  void SetTimeout(Time v);
  std::string GetStringHashValueFromQp();
  uint64_t GetBytesLeft();
  uint32_t GetHash(void);
  void Acknowledge(uint64_t ack);
  void ResumeQueue();
  void RecoverQueue();
  void RecoverQueueUponTimeout();
  uint64_t GetOnTheFly();
  bool IsWinBound();
  uint64_t GetWin(); // window size calculated from m_rate
  bool IsFinished();
  inline bool IsFinishedConst() const { return snd_una >= m_size; }
	void RecoverQueueLaps();
	void AcknowledgeForLaps(uint64_t ack, uint32_t sack_seq, uint16_t sack_sz, uint32_t fpid);
	uint64_t GetOnTheFlyForLaps();
	uint64_t GetWinForLaps();
	bool IsWinBoundForLaps();
		uint64_t GetBytesLeftForLaps();
	bool CanIrnTransmitForLaps(uint32_t mtu);
	void CheckAndUpdateQpStateForLaps();

  // callback for set rto
  typedef Callback<void, Ptr<RdmaQueuePair>, uint32_t, Time> RtoSet;
  RtoSet m_rtoSetCb;

  typedef Callback<Time, uint32_t> GetRtoTimeForPath;
  GetRtoTimeForPath m_cb_getRtoTimeForPath;

  typedef Callback<void, Ptr<RdmaQueuePair>, uint32_t> CancelRtoForPath;
  CancelRtoForPath m_cb_cancelRtoForPath;

  typedef Callback<bool, uint32_t> IsPathsValid;
  IsPathsValid m_cb_isPathsValid;

  typedef Callback<Time, uint32_t> getNxtAvailTimeForQp_t;
  getNxtAvailTimeForQp_t m_cb_getNxtAvailTimeForQp;

	uint64_t HpGetCurWin(); // window size calculated from hp.m_curRate, used by HPCC
};

class RdmaRxQueuePair : public Object { // Rx side queue pair
public:
	struct ECNAccount{
		uint16_t qIndex;
		uint8_t ecnbits;
		uint16_t qfb;
		uint16_t total;

		ECNAccount() { memset(this, 0, sizeof(ECNAccount));}
	};
	ECNAccount m_ecn_source;
	uint32_t sip, dip;
	uint16_t sport, dport;
	uint16_t m_ipid;
	uint32_t ReceiverNextExpectedSeq;
	Time m_nackTimer;
	int32_t m_milestone_rx;
	uint32_t m_lastNACK;
	EventId QcnTimerEvent; // if destroy this rxQp, remember to cancel this timer
	IrnSackManager m_sack;
	int32_t m_flow_id;
	static TypeId GetTypeId (void);
	RdmaRxQueuePair();
	uint32_t GetHash(void);
	std::string GetStringHashValueFromQp();
};

class RdmaQueuePairGroup : public Object {
public:
	std::vector<Ptr<RdmaQueuePair> > m_qps;
	//std::vector<Ptr<RdmaRxQueuePair> > m_rxQps;

	static TypeId GetTypeId (void);
	RdmaQueuePairGroup(void);
	uint32_t GetN(void);
	Ptr<RdmaQueuePair> Get(uint32_t idx);
	Ptr<RdmaQueuePair> operator[](uint32_t idx);
	void AddQp(Ptr<RdmaQueuePair> qp);
	//void AddRxQp(Ptr<RdmaRxQueuePair> rxQp);
	void Clear(void);

  char m_qp_finished[BITNSLOTS(MAX_RDMA_FLOW_PER_HOST)];
	inline bool IsQpFinished(uint32_t idx) {
			if (__glibc_unlikely(idx >= MAX_RDMA_FLOW_PER_HOST)) return false;
			return BITTEST(m_qp_finished, idx);
	}

	inline void SetQpFinished(uint32_t idx) {
			if (__glibc_unlikely(idx >= MAX_RDMA_FLOW_PER_HOST)) return;
			BITSET(m_qp_finished, idx);
	}


};

}

#endif /* RDMA_QUEUE_PAIR_H */
