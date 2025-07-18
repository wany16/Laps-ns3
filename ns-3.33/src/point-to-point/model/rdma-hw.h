#ifndef RDMA_HW_H
#define RDMA_HW_H

#include <ns3/rdma.h>
#include <ns3/rdma-queue-pair.h>
#include <ns3/node.h>
#include <ns3/custom-header.h>
#include "qbb-net-device.h"
#include <unordered_map>
#include "pint.h"
#include "common-user-model.h"
namespace ns3
{

		enum CongestionControlMode {
			/// Packet dropped due to missing route to the destination
			HPCC_PINT = 10,
			DCTCP = 8,
			TIMELY = 7,
			HPCC = 3,
			DCQCN_MLX = 1,
			CC_LAPS = 9,
			CC_NONE = 2,
		};




		enum ReceiverSequenceCheckResult {
			/// Packet dropped due to missing route to the destination
			ACTION_ERROR = 0, // 
			ACTION_ACK = 1, // expected && GBN && (expSeq>milestone || expSeq%chunk=0); useless && GBN
			ACTION_NACK = 2, // expected && IRN  && block exists; OoO && IRN && (packet-newly || nack-expired); GBN && (nack-expired || expSeq!=lastNack); useless && IRN && exist block
			ACTION_NACKED = 4, // OoO && IRN && duplicated && nacked; OoO && GBN && (nacked && expSeq=lastNack)
			ACTION_NACK_PSEUDO = 6, // expected && IRN && no block; useless && IRN && no block; 
			ACTION_NO_REPLY = 5, // expected && GBN && (expSeq<=milestone && expSeq%chunk!=0); 
		};



	struct RdmaInterfaceMgr
	{
		Ptr<QbbNetDevice> dev;
		Ptr<RdmaQueuePairGroup> qpGrp;

		RdmaInterfaceMgr() : dev(NULL), qpGrp(NULL) {}
		RdmaInterfaceMgr(Ptr<QbbNetDevice> _dev)
		{
			dev = _dev;
		}
	};

	class RdmaHw : public Object
	{
	public:
		static TypeId GetTypeId(void);
		RdmaHw();
		std::map<uint32_t, std::list<OutStandingDataEntry>> m_outstanding_data;
		static std::map<uint32_t, pstEntryData *> flowToPstEntry;
		bool isPathAvailable(uint32_t flowId);
		std::map<std::pair<uint32_t, uint32_t>, EventId> m_rtoEvents;
		std::map<uint32_t, EventId> m_rtoEventsPerPath;
  	void HandleTimeoutForLaps(Ptr<RdmaQueuePair> qp, uint32_t pid) ;
		void SetTimeoutForLaps(Ptr<RdmaQueuePair> qp, uint32_t pid, Time timeInNs); 
		static void printRateRecordToFile(std::string fileName);
		static void printPathDelayRecordToFile(std::string fileName);
        static std::map<uint32_t,uint64_t> m_RecordBuffSize;
		void CheckTxCompletedQp(uint16_t sport);

		static uint32_t qpFlowIndex;
		// static bool isIrnEnabled;
		std::unordered_map<unsigned, unsigned> m_cnt_timeout;
		bool LostPacketTest(uint32_t sepNum, std::string flowId);
		bool m_initFlowRateChange;
		// bool UpdateLostPacket(uint32_t sepNum,std::string flowId);
		static std::map<uint32_t, std::map<std::string, std::map<uint64_t, RecordCcmodeOutEntry>>> ccmodeOutInfo; // Record CCMod outinfo:send rate\RTO evente
		std::map<uint32_t, std::map<std::string, uint32_t>> m_packetLost;										  // node->flowid->packetSeq;
		static std::map<uint32_t, std::map<std::string, std::map<uint32_t, LostPacketEntry>>> m_lossPacket;		  // QPId->packetSeqNum->lostNum
		static std::map<std::string, std::string> m_recordQpSen;												  //  ->snd_una and snd_una-m_size
		static std::map<uint32_t, QpRecordEntry> m_recordQpExec;

		static std::map<std::string, std::vector<RecordFlowRateEntry_t>> m_qpRatechange;

		static uint32_t flowComplteNum;
		static std::map<uint32_t, std::vector<RecordFlowRateEntry_t>> recordRateMap;		  //
		static std::map<uint32_t, std::vector<RecordPathDelayEntry_t>> recordPathDelayMap; //
		static std::map<uint32_t, uint64_t> pidToThDelay;		  //
		static bool enableRateRecord;
		static bool enablePathDelayRecord;
		static void insertRateRecord(uint32_t flowId, uint64_t curRateInMbps);
		static void insertPathDelayRecord(uint32_t pid, uint64_t pathDelayInNs);
		static std::map<uint32_t, uint64_t> RecordPacketHop;
		Ptr<Node> m_node;
		DataRate m_minRate; //< Min sending rate
		uint32_t m_mtu;
		CongestionControlMode m_cc_mode;
		double m_nack_interval;
		uint32_t m_chunk;
		uint32_t m_ack_interval;
		bool m_backto0;
		bool m_var_win, m_fast_react;
		bool m_rateBound;
		std::vector<RdmaInterfaceMgr> m_nic;					  // list of running nic controlled by this RdmaHw
		std::unordered_map<uint64_t, Ptr<RdmaQueuePair>> m_qpMap; // mapping from uint64_t to qp
		std::unordered_map<uint64_t, uint64_t> m_finishedQpMap;

		std::unordered_map<uint64_t, Ptr<RdmaRxQueuePair>> m_rxQpMap; // mapping from uint64_t to rx qp
		std::unordered_map<uint32_t, std::vector<int>> m_rtTable;	  // map from ip address (u32) to possible ECMP port (index of dev)

		// qp complete callback
		typedef Callback<void, Ptr<RdmaQueuePair>> QpCompleteCallback;
		QpCompleteCallback m_qpCompleteCallback;
		void HandleTimeout(Ptr<RdmaQueuePair> qp, Time rto);

		void SetNode(Ptr<Node> node);
		void Setup(QpCompleteCallback cb);																																					 // setup shared data and callbacks with the QbbNetDevice
		static uint64_t GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg);																												 // get the lookup key for m_qpMap
		Ptr<RdmaQueuePair> GetQp(uint64_t key);																												 // get the qp
		uint32_t GetNicIdxOfQp(Ptr<RdmaQueuePair> qp);																																		 // get the NIC index of the qp
		void AddQueuePair(uint64_t size, uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport, uint32_t win, uint64_t baseRtt, int32_t flowId, Callback<void> notifyAppFinish); // add a new qp (new send)
		void DeleteQueuePair(Ptr<RdmaQueuePair> qp);

	uint64_t GetRxQpKey(uint32_t dip, uint16_t dport, uint16_t sport, uint16_t pg) ;
		Ptr<RdmaRxQueuePair> GetRxQp(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg, bool create); // get a rxQp
		uint32_t GetNicIdxOfRxQp(Ptr<RdmaRxQueuePair> q);																	// get the NIC index of the rxQp
		void DeleteRxQp(uint32_t dip, uint16_t dport, uint16_t sport, uint16_t pg);
	Ptr<RdmaRxQueuePair> InitRxQp(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg, int32_t flowId);
		int ReceiveUdp(Ptr<Packet> p, CustomHeader &ch);
		int ReceiveCnp(Ptr<Packet> p, CustomHeader &ch);
		int ReceiveAck(Ptr<Packet> p, CustomHeader &ch); // handle both ACK and NACK
		int Receive(Ptr<Packet> p, CustomHeader &ch);	 // callback function that the QbbNetDevice should use when receive packets. Only NIC can call this function. And do not call this upon PFC

		void CheckandSendQCN(Ptr<RdmaRxQueuePair> q);
		ReceiverSequenceCheckResult ReceiverCheckSeq(uint32_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size, bool & cnp);
		void AddHeader(Ptr<Packet> p, uint16_t protocolNumber);
		static uint16_t EtherToPpp(uint16_t protocol);
		Ptr<Packet> ConstructAckForUDP(ReceiverSequenceCheckResult state, const CustomHeader &ch, Ptr<RdmaRxQueuePair> rxQp, uint32_t udpPayloadSize);
		void RecoverQueue(Ptr<RdmaQueuePair> qp);
		
		void QpComplete(Ptr<RdmaQueuePair> qp);
		void SetLinkDown(Ptr<QbbNetDevice> dev);
		Ptr<RdmaSmartFlowRouting> GetE2ELapsLBouting();
		Ptr<RdmaSmartFlowRouting> m_E2ErdmaSmartFlowRouting;
		LB_Solution m_lbSolution;
		// call this function after the NIC is setup
		void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
		void ClearTable();
		void RedistributeQp();

		Ptr<Packet> GetNxtPacket(Ptr<RdmaQueuePair> qp); // get next packet to send, inc snd_nxt
		void PktSent(Ptr<RdmaQueuePair> qp, Ptr<Packet> pkt, Time interframeGap);
		void LBPktSent(Ptr<RdmaQueuePair> qp, uint32_t pkt_size, Time interframeGap); // For E2E LB
		void UpdateNextAvail(Ptr<RdmaQueuePair> qp, Time interframeGap, uint32_t pkt_size);
		void ChangeRate(Ptr<RdmaQueuePair> qp, DataRate new_rate);
		/******************************
		 * Mellanox's version of DCQCN
		 *****************************/
		double m_g;				 // feedback weight
		double m_rateOnFirstCNP; // the fraction of line rate to set on first CNP
		bool m_EcnClampTgtRate;
		double m_rpgTimeReset;
		double m_rateDecreaseInterval;
		uint32_t m_rpgThreshold;
		double m_alpha_resume_interval;
		DataRate m_rai;	 //< Rate of additive increase
		DataRate m_rhai; //< Rate of hyper-additive increase
        uint32_t  flowPerHost;
		uint32_t m_cnt_cnpByEcn;
		uint32_t m_cnt_cnpByOoo;
		uint32_t m_cnt_Cnp;

		// Implement Timeout according to IB Spec Vol. 1 C9-139.
		// For an HCA requester using Reliable Connection service, to detect missing responses,
		// every Send queue is required to implement a Transport Timer to time outstanding requests.
		Time m_waitAckTimeout;

		// std::unordered_map<uint32_t, bool> m_manualDropSeqMap={{2000, true},{3000, true},{9000,true},{19000,true}};

		std::unordered_map<uint32_t, bool> m_manualDropSeqMap1 = {{2000, true}};
		std::unordered_map<uint32_t, bool> m_manualDropSeqMap2 = {{1000, true}, {2000, true}};
		std::unordered_map<uint32_t, bool> m_manualDropSeqMap3 = {{3000, true}};
		// the Mellanox's version of alpha update:
		// every fixed time slot, update alpha.
		void UpdateAlphaMlx(Ptr<RdmaQueuePair> q);
		void ScheduleUpdateAlphaMlx(Ptr<RdmaQueuePair> q);

		// Mellanox's version of CNP receive
		void cnp_received_mlx(Ptr<RdmaQueuePair> q);

		// Mellanox's version of rate decrease
		// It checks every m_rateDecreaseInterval if CNP arrived (m_decrease_cnp_arrived).
		// If so, decrease rate, and reset all rate increase related things
		void CheckRateDecreaseMlx(Ptr<RdmaQueuePair> q);
		void ScheduleDecreaseRateMlx(Ptr<RdmaQueuePair> q, uint32_t delta);

		// Mellanox's version of rate increase
		void RateIncEventTimerMlx(Ptr<RdmaQueuePair> q);
		void RateIncEventMlx(Ptr<RdmaQueuePair> q);
		void FastRecoveryMlx(Ptr<RdmaQueuePair> q);
		void ActiveIncreaseMlx(Ptr<RdmaQueuePair> q);
		void HyperIncreaseMlx(Ptr<RdmaQueuePair> q);

		/***********************
		 * High Precision CC
		 ***********************/
		double m_targetUtil;
		double m_utilHigh;
		uint32_t m_miThresh;
		bool m_multipleRate;
		bool m_sampleFeedback; // only react to feedback every RTT, or qlen > 0
		void HandleAckHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);
		void UpdateRateHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react);
		void UpdateRateHpTest(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react);
		void FastReactHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);

		/**********************
		 * TIMELY
		 *********************/
		double m_tmly_alpha, m_tmly_beta;
		uint64_t m_tmly_TLow, m_tmly_THigh, m_tmly_minRtt;
		void HandleAckTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);
		void UpdateRateTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool us);
		void FastReactTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);

		/**********************
		 * DCTCP
		 *********************/
		DataRate m_dctcp_rai;
		void HandleAckDctcp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);
		std::map<std::string, PlbEntry> m_plbtable;
		PlbEntry *lookup_PlbEntry(std::string flowId);
		void PlbUpdateState(Ptr<RdmaQueuePair> qp);
		void plbCheckRehash(Ptr<RdmaQueuePair> qp);
		void plb_update_state_upon_rto(Ptr<RdmaQueuePair> qp);
		uint32_t GetFlowIdRehashNum(std::string flowId);
		void PLBHandleAckDctcp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);

		static std::map<uint32_t, std::map<uint32_t, PlbRecordEntry>> m_plbRecordOutInf; // nodeid->flowID

		/*********************
		 * HPCC-PINT
		 ********************/
		uint32_t pint_smpl_thresh;
		void SetPintSmplThresh(double p);
		void HandleAckHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);
		void UpdateRateHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react);

		/*********************
		 * LAPS
		 ********************/
		void HandleAckLaps(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch);
		int ReceiveUdpOnDstHostForLaps(Ptr<Packet> p, CustomHeader &ch);
		bool checkQpFinishedOnDstHost(const CustomHeader &ch);
		ReceiverSequenceCheckResult ReceiverCheckSeqForLaps(uint32_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size, bool &cnp);
		// void HandleTimeoutForLaps(Ptr<RdmaQueuePair> qp);
	void AddQueuePairForLaps(uint64_t size, uint16_t pg, Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport, uint32_t win, uint64_t baseRtt, int32_t flowId, Callback<void> notifyAppFinish);
	void UpdateRateForLaps(Ptr<RdmaQueuePair> qp, CustomHeader &ch);
	int ReceiveAckForLaps(Ptr<Packet> p, CustomHeader &ch);
	int ReceiveForLaps(Ptr<Packet> p, CustomHeader &ch);
	bool checkRxQpFinishedOnDstHost(const CustomHeader &ch);
	int64_t IncreaseRateForLaps(Ptr<RdmaQueuePair> qp, uint64_t nxtIncTimeInNs);
	int64_t DecreaseRateForLaps(Ptr<RdmaQueuePair> qp, uint64_t nxtDecTimeInNs);
	void UpdateNxtQpAvailTimeForLaps(Ptr<RdmaQueuePair> qp, int64_t timeGap);
	Time GetRtoTimeForPath(uint32_t pathId);
	void CancelRtoForPath(Ptr<RdmaQueuePair> qp, uint32_t pathId);
	void AppendOutStandingDataPerPath(uint32_t pathId, OutStandingDataEntry  e);
	bool checkOutstandingDataAndUpdateLossyData(uint32_t pid, uint32_t flowId, uint32_t seq, uint16_t size);
	void HandleTimeoutForLapsPerPath(uint32_t pid) ;
	void SetTimeoutForLapsPerPath(uint32_t pid) ;
	void CancelRtoPerPath(uint32_t pathId);
	Ptr<Packet> ConstructAckForProbe(const CustomHeader &ch);
	int ReceiveProbeDataOnDstHostForLaps(Ptr<Packet> p, CustomHeader &ch);
	int ReceiveProbeAckForLaps(Ptr<Packet> p, CustomHeader &ch);
	// bool isPathAvailable(uint32_t flowId);
	Time getNxtAvailTimeForQp(uint32_t flowId);




	};

} /* namespace ns3 */

#endif /* RDMA_HW_H */
