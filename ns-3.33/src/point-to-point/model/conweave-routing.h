#ifndef __CONWEAVE_ROUTING_H__
#define __CONWEAVE_ROUTING_H__
#include <iostream>
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/net-device.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/tag.h"
#include "ns3/common-user-model.h"

namespace ns3
{
#define CW_DEFAULT_32BIT (UINT32_MAX)
#define CW_DEFAULT_64BIT (Seconds(100).GetNanoSeconds())
#define CW_MAX_TIME (Seconds(100))
#define CW_MIN_TIME (Seconds(0))
    const uint32_t CONWEAVE_CTRL_DUMMY_INDEV = 1;
    /************************************************************************************************
     * #B# We use "tag" instead of "header" class in NS-3. Since ConWeave header's extra overhead is
     *     very small, we believe its effect on performance is negligible.
     *
     *
     * ---- TERMINOLOGY: ----
     * Some terminologies are a bit different with the paper:
     *
     * (1) Epoch (Big Loop): starts a new epoch whenever there is no remaining out-of-order packet of
     * this message (epoch in paper)
     *
     * (2) Phase is the tag REROUTE in paper.
     *     0: before rerouting, 1: after rerouting, within a specific epoch
     *
     * (3) Phase0Cache: to check there is no prior packet that arrive before REROUTE (see Appendix A)
     *
     * (4) "Stabilized" means no on-going packet reordering, so can start new rerouting
     *
     * (5) Reply + tag(INIT) = RTT_REPLY, Reply + tag(TAIL) = CLEAR in paper
     *
     * (6) Data + tag(INIT) = RTT_REQUEST, Data + tag(TAIL) = TAIL in paper
     *
     * ************************************************************************************************
     */

    struct conweaveTxState
    {
        std::string pkt_flowkey;
        bool _stabilized = true; /* make new connection always starts with epoch 1 */
        Time _activeTime = CW_MIN_TIME;
        Time _replyTimer = CW_MIN_TIME;
        uint32_t _epoch = 0;                 /* by expiration at the beginning, the first packet begins with epoch 1 */
        uint32_t _phase = 0;                 /* 0: before rerouting, 1: after rerouting */
        uint32_t _pathId = CW_DEFAULT_32BIT; /* encoded current path ID (composition of uplinks) */
        Time _tailTime = CW_MIN_TIME;        /* TAIL packet of current epoch (if available) */
    };

    struct conweaveRxState
    {
        std::string pkt_flowkey;
        Time _activeTime = CW_MIN_TIME; /* for auto-deletion (aging) - NS3 specific */
        uint32_t _epoch = 1;            /* Rx's epoch starts with 1, because Tx always starts with epoch 1 */
        uint32_t _phase = 0;
        Time _phase0TxTime = CW_MIN_TIME; /* TX_TSTAMP (at srcToR) of packets through OLD path  */
        Time _phase0RxTime = CW_MIN_TIME; /* RX_TSTAMP (at dstToR) of packets through OLD path */
        bool _phase0Cache =
            false;                    /* to check whether there is prior packets that arrive before REROUTED */
        Time _tailTime = CW_MIN_TIME; /* TX_TAIL_TSTAMP of current epoch */
        bool _reordering = false;     /* For VOQ */
    };

    struct conweavePathInfo
    {
        uint32_t _pathId = 0;
        Time _invalidTime = CW_MIN_TIME;
    };

    struct find_conweavePathInfo
    {
        uint32_t _pathId;
        find_conweavePathInfo(uint32_t pathId) : _pathId(pathId) {}
        bool operator()(const conweavePathInfo &p) const { return p._pathId == _pathId; }
    };

    // follow PISA metadata concept
    struct conweaveTxMeta
    {
        std::string pkt_flowkey;
        bool newConnection = false;
        bool flagExpired = false;
        bool flagReplyTimeout = false;
        bool flagStabilized = false;
        uint32_t epoch = 0;
        uint32_t phase = 0;
        uint32_t goodPath = CW_DEFAULT_32BIT;
        bool foundGoodPath = false;
        uint32_t currPath = CW_DEFAULT_32BIT;
        uint64_t tailTime = 0;

        /*-- REPLY Metadata --*/
        uint32_t reply_flag;
        uint32_t reply_epoch;
        uint32_t reply_phase;
    };

    // follow PISA metadata concept
    struct conweaveRxMeta
    {
        std::string pkt_flowkey;
        uint32_t pkt_pathId = 0;
        uint32_t pkt_epoch = 0;
        uint32_t pkt_phase = 0;
        uint64_t pkt_timestamp_Tx = 0;
        uint64_t pkt_timestamp_TAIL = 0;
        uint32_t pkt_flagData = 0;
        uint8_t pkt_ecnbits = 0; /* ECN bits */
        bool newConnection = false;
        uint8_t resultEpochMatch = 0; /* 1: new epoch, 2: prev epoch, 0: curr epoch */
        Time phase0TxTime;
        Time phase0RxTime;
        bool flagOutOfOrder = false;
        bool flagPhase0Cache = false; /* to check whether RTT info is available or not */
        uint64_t tailTime = 0;
        uint64_t timegapAtTx = 0;
        uint64_t timeExpectedToFlush = 0;
        bool flagEnqueue = false;
    };
    class ConWeaveVOQ
    {
        friend class ConWeaveRouting;

    public:
        ConWeaveVOQ();
        ~ConWeaveVOQ();

        // functions
        void Set(std::string flowkey, uint32_t dip, Time timeToFlush, Time extraVOQFlushTime); // setup
        void Enqueue(Ptr<Packet> pkt);                                                         // enqueue pkt FIFO
        void FlushAllImmediately();                                                            // flush all immediately (for scheduling)
        void EnforceFlushAll();                                                                // enforce to flush the queue by timeout (makes OoO)
        void RescheduleFlush(Time timeToFlush);                                                // reschedule timeout to flush
        bool CheckEmpty();                                                                     // check empty
        uint32_t getQueueSize();                                                               // get queue size
        uint32_t getDIP() { return m_dip; };

        // logging
        static std::vector<int> m_flushEstErrorhistory;

    private:
        std::string m_flowkey;          // flowkey (voqMap's key)
        uint32_t m_dip;                 // destination ip (for monitoring)
        std::queue<Ptr<Packet>> m_FIFO; // per-flow FIFO queue
        EventId m_checkFlushEvent;      // check flush schedule is on-going (will be false once the queue
                                        // starts flushing)
        Time m_extraVOQFlushTime;       // extra flush time (for network uncertainty) -- for debugging

        // callback
        Callback<void, std::string> m_deleteCallback; // bound to SlbRouting::DeleteVoQ
        Callback<void, std::string, uint32_t>
            m_CallbackByVOQFlush; // bound to SlbRouting::CallbackByVOQFlush
        Callback<void, Ptr<Packet>, CustomHeader &>
            m_switchSendToDevCallback; // bound to SlbRouting::DoSwitchSendToDev
    };

    /*----------------------------*/
    // tag for data and request
    class ConWeaveDataTag : public Tag
    {
    public:
        ConWeaveDataTag();
        void SetPathId(uint32_t pathId);
        uint32_t GetPathId(void) const;
        void SetHopCount(uint32_t hopCount);
        uint32_t GetHopCount(void) const;
        void SetEpoch(uint32_t epoch);
        uint32_t GetEpoch(void) const;
        void SetPhase(uint32_t phase);
        uint32_t GetPhase(void) const;
        void SetTimestampTx(uint64_t timestamp);
        uint64_t GetTimestampTx(void) const;
        void SetTimestampTail(uint64_t timestamp);
        uint64_t GetTimestampTail(void) const;
        void SetFlagData(uint32_t flag);
        uint32_t GetFlagData(void) const;

        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

        friend std::ostream &operator<<(std::ostream &os, ConWeaveDataTag const &tag)
        {
            return os << "m_pathId:" << tag.m_pathId << "\n"
                      << "m_hopCount:" << tag.m_hopCount << "\n"
                      << "m_epoch:" << tag.m_epoch << "\n"
                      << "m_phase:" << tag.m_phase << "\n"
                      << "m_timestampTx:" << tag.m_timestampTx << "\n"
                      << "m_timestampTail:" << tag.m_timestampTail << "\n"
                      << "m_flagData:" << tag.m_flagData << "\n"
                      << std::endl;
        }

        enum ConWeaveDataTagFlag
        {
            NONE = 0, /* Default, unused */
            DATA = 1,
            INIT = 2,
            TAIL = 3,
        };

    private:
        uint32_t m_pathId;
        uint32_t m_hopCount;
        uint32_t m_epoch;
        uint32_t m_phase;
        uint64_t m_timestampTx;                      // departure time at TxToR
        uint64_t m_timestampTail;                    // time of last packet in previous epoch
        uint32_t m_flagData = ConWeaveDataTag::NONE; // control flag
    };

    // tag for reply
    class ConWeaveReplyTag : public Tag
    {
    public:
        ConWeaveReplyTag();
        void SetFlagReply(uint32_t flagReply);
        uint32_t GetFlagReply(void) const;
        void SetEpoch(uint32_t epoch);
        uint32_t GetEpoch(void) const;
        void SetPhase(uint32_t phase);
        uint32_t GetPhase(void) const;

        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

        friend std::ostream &operator<<(std::ostream &os, ConWeaveReplyTag const &tag)
        {
            return os << "m_flagReply:" << tag.m_flagReply << "\n"
                      << "m_epoch:" << tag.m_epoch << "\n"
                      << "m_phase:" << tag.m_phase << "\n"
                      << std::endl;
        }

        enum ConWeaveReplyTagFlag
        {
            NONE = 0, /* Default, unused */
            INIT = 1, /* RTT_REPLY in paper */
            TAIL = 2, /* CLEAR in paper */
        };

    private:
        uint32_t m_flagReply;
        uint32_t m_epoch;
        uint32_t m_phase;
    };

    // tag for congestion NOTIFY packet
    class ConWeaveNotifyTag : public Tag
    {
    public:
        ConWeaveNotifyTag();
        void SetPathId(uint32_t pathId);
        uint32_t GetPathId(void) const;

        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

        friend std::ostream &operator<<(std::ostream &os, ConWeaveNotifyTag const &tag)
        {
            return os << "m_pathId:" << tag.m_pathId << std::endl;
        }

    private:
        uint32_t m_pathId; // path of DATA
    };

    class ConWeaveRouting : public Object
    {

    public:
        ConWeaveRouting();
        ~ConWeaveRouting();
        virtual void DoDispose();

        /* static */
        static TypeId GetTypeId(void);
        static uint32_t GetOutPortFromPath(
            const uint32_t &path,
            const uint32_t &hopCount); // decode outPort from path, given a hop's order
        static void SetOutPortToPath(uint32_t &path, const uint32_t &hopCount,
                                     const uint32_t &outPort); // encode outPort to path

        /* key */
        static uint64_t GetFlowKey(uint32_t ip1, uint32_t ip2, uint16_t port1,
                                   uint16_t port2);                            // hashkey (4-tuple)
        static uint32_t DoHash(const uint8_t *key, size_t len, uint32_t seed); // hash function
        uint32_t GetNumVOQ() { return (uint32_t)m_voqMap.size(); }
        uint32_t GetVolumeVOQ();
        const std::unordered_map<std::string, ConWeaveVOQ> &GetVOQMap() { return m_voqMap; }
        void PrintConweaveTxTable();
        /* main function */

        void SendReply(Ptr<Packet> p, CustomHeader &ch, uint32_t flagReply, uint32_t pkt_epoch);
        void SendNotify(Ptr<Packet> p, CustomHeader &ch, uint32_t pathId);
        void forwardSpeicalPackets(Ptr<Packet> p, CustomHeader &ch, bool foundConWeaveReplyTag, bool foundConWeaveNotifyTag, bool IsSrcToREqualdstToR, bool &IsSend);
        void RouteInput(Ptr<Packet> p, CustomHeader &ch); // core function
        void initializeConweaveTxData(conweaveTxState &txEntry, conweaveTxMeta &tx_md, HostId2PathSeleKey pstKey, CustomHeader &ch);
        void pathSelect(conweaveTxState &txEntry, conweaveTxMeta &tx_md, HostId2PathSeleKey pstKey, CustomHeader &ch);
        void forwardSrctorPacket(ConWeaveDataTag &conweaveDataTag, Ptr<Packet> p, CustomHeader &ch, conweaveTxMeta &tx_md);
        void initializeConweaveRxData(conweaveRxState &rxEntry, conweaveRxMeta &rx_md, ConWeaveDataTag &conweaveDataTag, CustomHeader &ch);
        void epochMatch(conweaveRxState &rxEntry, conweaveRxMeta &rx_md, Ptr<Packet> p, CustomHeader &ch);
        void epochToCheck(conweaveRxState &rxEntry, conweaveRxMeta &rx_md, Ptr<Packet> p, CustomHeader &ch);
        void updateExpectedToFlushTime(conweaveRxState &rxEntry, conweaveRxMeta &rx_md, CustomHeader &ch);
        void phaseToDisposeVqq(conweaveRxState &rxEntry, conweaveRxMeta &rx_md, CustomHeader &ch);
        void replyLablePacket(conweaveRxMeta &rx_md, Ptr<Packet> p, CustomHeader &ch);
        void disposeReplyPacket(ConWeaveReplyTag &conweaveReplyTag, bool foundConWeaveReplyTag, CustomHeader &ch, ConWeaveNotifyTag &conweaveNotifyTag, bool foundConWeaveNotifyTag);
        void onlyForwardPacket(ConWeaveDataTag &conweaveDataTag, Ptr<Packet> p, CustomHeader &ch);
        uint32_t get_the_path_length_by_path_id(const uint32_t pathId);
        bool reach_the_last_hop_of_path_tag(ConWeaveDataTag &conweaveDataTag);
        void DeleteVOQ(std::string flowkey); // used for callback when reorder queue is flushed
        EventId m_agingEvent;
        EventId m_recordEvent;
        void AgingEvent(); // aging Tx/RxTableEntry (for cleaning and improve NS-3 simulation)

        /* SET functions */
        void SetConstants(Time extraReplyDeadline, Time extraVOQFlushTime, Time txExpiryTime,
                          Time defaultVOQWaitingTime, Time pathPauseTime, bool pathAwareRerouting);
        void SetSwitchInfo(bool isToR, uint32_t switch_id);

        // callback of SwitchSend
        void DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev,
                          uint32_t qIndex);                      // TxToR and Agg/CoreSw
        void DoSwitchSendToDev(Ptr<Packet> p, CustomHeader &ch); // only at RxToR

        void CallbackByVOQFlush(std::string flowkey, uint32_t voqSize); // used for callback in VOQ

        typedef Callback<void, Ptr<Packet>, CustomHeader &, uint32_t, uint32_t> SwitchSendCallback;
        typedef Callback<void, Ptr<Packet>, CustomHeader &> SwitchSendToDevCallback;
        void SetSwitchSendCallback(SwitchSendCallback switchSendCallback); // set callback
        void SetSwitchSendToDevCallback(
            SwitchSendToDevCallback switchSendToDevCallback); // set callback

        /* topological info (should be initialized in the beginning) */
        std::map<uint32_t, std::set<uint32_t>>
            m_ConWeaveRoutingTable;                        // <RxToRId -> set<pathId> > just for reference
        std::map<uint32_t, uint64_t> m_rxToRId2BaseRTT;    // RxToRId -> BaseRTT between TORs(fixed)
        std::vector<conweavePathInfo> m_conweavePathTable; // pathInfo table
        // std::map<Ipv4Address, hostIp2SMT_entry_t> m_vmVtepMapTbl;
        static RoutePath routePath;

        /* statistics (logging) */
        static std::map<HostId2PathSeleKey, std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>>> m_recordPath; // timegap->pid->sendpacketsize
        static uint64_t m_nReplyInitSent;              // number of reply sent
        static uint64_t m_nReplyTailSent;              // number of reply sent
        static uint64_t m_nTimelyInitReplied;          // number of reply timely arrived at TxToR
        static uint64_t m_nTimelyTailReplied;          // number of reply timely arrived at TxToR
        static uint64_t m_nNotifySent;                 // number of feedback sent
        static uint64_t m_nReRoute;                    // number of rerouting path by Flowcut
        static uint64_t m_nOutOfOrderPkts;             // number of OoO packets and queued at VOQ
        static uint64_t m_nFlushVOQTotal;              // number of VOQ flush by timeout (can cause out-of-order)
        static uint64_t m_nFlushVOQByTail;             // number of flushing VOQ natually (w/o out-of-order issue)
        static std::vector<uint32_t> m_historyVOQSize; // history of VOQ size
        void RecordPathload();
        void updatePathLoad(uint32_t size, uint32_t pathId);

    private:
        // callback
        SwitchSendCallback m_switchSendCallback; // bound to SwitchNode::SwitchSend (for Request/UDP)
        SwitchSendToDevCallback
            m_switchSendToDevCallback; // bound to SwitchNode::SendToDevContinue (for Probe, Reply)

        // topology parameters
        bool m_isToR;         // is ToR (leaf)
        uint32_t m_switch_id; // switch's nodeID

        // conweave parameters
        Time m_extraReplyDeadline; // additional term to reply deadline
        Time m_extraVOQFlushTime;  // extra for uncertainty
        Time m_txExpiryTime;       // flowlet timegap
        Time m_defaultVOQWaitingTime;
        Time m_pathPauseTime; // time to pause path selection when getting ECN's feedback
        bool m_pathAwareRerouting;
        Time m_agingTime; // aging time (e.g., 2ms)
        Time m_recordTime = MilliSeconds(10);
        // local
        std::map<std::string, conweaveTxState> m_conweaveTxTable; // flowkey -> TxToR's stateful table
        std::map<std::string, conweaveRxState> m_conweaveRxTable; // flowkey -> RxToR's stateful table

        // VOQ (voq.m_deleteCallback = MakeCallback(&ConWeaveRouting::deleteVoq, this); )
        std::unordered_map<std::string, ConWeaveVOQ> m_voqMap; // flowkey -> FIFO Queue
        uint32_t recordNum=0;
        static uint64_t debug_time;
    };

} // namespace ns3
#endif