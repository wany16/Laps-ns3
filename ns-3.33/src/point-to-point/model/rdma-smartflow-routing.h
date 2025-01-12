/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef RDMA_SMARTFLOW_ROUTING_H
#define RDMA_SMARTFLOW_ROUTING_H

#include "ns3/ipv4-smartflow-tag.h"
#include "ns3/callback.h"
#include <stdexcept>
#include <algorithm>
#include <utility>
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/net-device.h"
#include "ns3/channel.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/boolean.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h" // conga
#include "ns3/object.h"
#include "common-user-model.h"
#include "rdma-queue-pair.h"
#include <random>


#define DEFAULT_PATH_ID 999999
#define DEFAULT_PATH_INDEX 999999999
#define DEFAULT_PATH_SELECTION_NUM 2
#define RDMA_LINK_LATENCY_IN_NANOSECOND 1
#define PROBE_SMALL_LATENCY_FIRST_STRATEGY 0
#define PROBE_SMALL_GENERATION_TIME_FIRST_STRATEGY 1
#define PROBE_RANDOM_STRATEGY 2
#define PROBE_PATH_EXPIRED_TIME_IN_NANOSECOND 1000000
#define PROBE_DEFAULT_INTERVAL_IN_NANOSECOND 1000000
#define PROBE_DEFAULT_PKT_SIZE_IN_BYTE 0

#define PIGGY_BACK_SMALL_LATENCY_FIRST_STRATEGY 0
#define PIGGY_BACK_SMALL_GENT_TIME_FIRST_STRATEGY 1
#define PIGGY_BACK_SMALL_SENT_TIME_FIRST_STRATEGY 2
#define PIGGY_BACK_DEFAULT_PATH_NUMBER 15

#define PATH_SELECTION_PRIORITY_FIRST_STRATEGY 1
#define PATH_SELECTION_ROUND_ROBIN_FOR_PACKET_STRATEGY 2
#define PATH_SELECTION_RANDOM_STRATEGY 3
#define PATH_SELECTION_SMALL_LATENCY_FIRST_STRATEGY 4
#define PATH_SELECTION_FLOWLET_STATEGY 5
#define PATH_SELECTION_FLOW_HASH_STRATEGY 6
#define PATH_SELECTION_ROUND_ROBIN_FOR_FLOW_STRATEGY 7
#define PATH_SELECTION_ROUND_ROBIN_FOR_HYBRID_STRATEGY 8
#define PATH_SELECTION_SMALL_LATENCY_FIRST_OPTIMIZED_STRATEGY 9
#define PATH_SELECTION_CONGA_STRATEGY 10

#define FLOWLET_DEFAULT_TIMEOUT_IN_NANOSECOND 100000

#define CONGA_DRE_DECRESE_TIME_IN_US 160 // conga
#define CONGA_EXPIRED_TIME_IN_US 200     // conga
#define CONGA_SW_PORT_NUM 10             // conga

#define REORDER_SAMPLE_RATIO 1

namespace ns3
{

  class E2ESrcOutPackets : public SimpleRefCount<E2ESrcOutPackets>
  {
  public:
    Ptr<Packet> dataPacket = NULL;
    Ptr<Packet> probePacket = NULL;
    Ptr<Packet> ackPacket = NULL;
    uint64_t latencyForDataPktInNs = 0;
    uint32_t pidForDataPkt = 0;
    
    bool isData = false;
    bool isProbe = false;
    bool isAck = false;

    Ptr<RdmaQueuePair> lastQp = NULL;
    E2ESrcOutPackets(Ptr<Packet> dataPacket, bool Isprobe, Ptr<Packet> probePacket)
    {
      this->dataPacket = dataPacket;
      this->probePacket = probePacket;
      this->isProbe = Isprobe;

    }
    E2ESrcOutPackets() : dataPacket(NULL), probePacket(NULL), ackPacket(NULL), isData(false), isProbe(false), isAck(false), lastQp(NULL) {}
  };

  class RdmaSmartFlowRouting : public Object
  {
    // friend class SwitchMmu;
    // friend class SwitchNode;

  public:
    RdmaSmartFlowRouting();
    virtual ~RdmaSmartFlowRouting();
    static std::vector<probeInfoEntry> m_prbInfoTable;
    static std::map<std::string, reorder_entry_t> m_reorderTable;
    static double laps_alpha;
    static std::uniform_real_distribution<double> rndGen;
    static std::default_random_engine generator;

  public:
    static TypeId GetTypeId(void);
    // virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
    // virtual bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);
    // Ptr<Ipv4Route> ConstructIpv4Route (uint32_t port, Ipv4Address &destAddress);
    bool RouteInput(Ptr<Packet> p, CustomHeader ch);
    void RouteOutput(Ptr<Packet> p, CustomHeader ch, Ptr<E2ESrcOutPackets> &SrcOutEntry); // E2E LB input
    bool e2eLBSrc_output_packet(Ptr<E2ESrcOutPackets> &SrcOutEntry);

    void add_latency_tag_by_pit_entries(Ptr<Packet> packet, std::vector<PathData *> &pitEntries);
    void add_path_tag_by_path_id(Ptr<Packet> packet, uint32_t pid);
    void add_probe_tag_by_path_id(Ptr<Packet> packet, uint32_t expiredPathId);
    std::vector<PathData *> batch_lookup_PIT(std::vector<uint32_t> &pids);
    Ipv4SmartFlowPathTag construct_path_tag(uint32_t selectedPathId);
    Ptr<Packet> construct_probe_packet(Ptr<Packet> &pkt, CustomHeader &ch);
    Ptr<Packet> construct_reply_probe_packet(Ptr<Packet> &pkt, CustomHeader &ch);
    Ipv4SmartFlowProbeTag construct_probe_tag_by_path_id(uint32_t expiredPathId);
    bool exist_path_tag(Ptr<Packet> packet, Ipv4SmartFlowPathTag &pathTag);
    bool exist_probe_tag(Ptr<Packet> packet, Ipv4SmartFlowProbeTag &probeTag);
    bool exist_ack_tag(Ptr<Packet> packet, AckPathTag &ackTag);

    uint32_t forward_normal_packet(Ptr<Packet> &p, CustomHeader &ch, uint32_t srcToRId, uint32_t dstToRId, uint32_t pg, Ptr<E2ESrcOutPackets> &SrcOutEntry);
    // uint32_t forward_probe_packet(Ptr<Packet> pkt, std::vector<PathData *> &forwardPitEntries, PathData *bestPitEntry, const Ipv4Header &header, UnicastForwardCallback ucb, Ipv4Address dstServerAddr);
    uint32_t forward_probe_packet_optimized(Ptr<Packet> pkt, std::vector<PathData *> &forwardPitEntries, CustomHeader &ch, uint32_t pg, Ptr<E2ESrcOutPackets> &SrcOutEntry);

    uint32_t get_egress_port_id_by_path_tag(Ipv4SmartFlowPathTag &smartFlowTag);
    // uint32_t get_node_id(void) const;
    uint32_t get_max_piggyback_path_number() const;
    uint32_t get_path_expire_interval() const;
    std::map<uint32_t, PathData> get_PIT() const;
    uint32_t get_probing_interval() const;
    std::map<HostId2PathSeleKey, pstEntryData> get_PST() const;
    // PathData *get_the_best_forwarding_path(std::vector<PathData *> &pitEntries, pstEntryData *pstEntry, Ptr<const Packet> packet, const Ipv4Header &header);
    std::vector<PathData *> get_the_best_piggyback_paths(std::vector<PathData *> &pitEntries);
    uint32_t get_the_expired_paths(std::vector<PathData *> &allPitEntries, std::vector<PathData *> &expiredPitEntries);
    // PathData *get_the_flowlet_path(pstEntryData *pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header);
    // PathData *get_the_hashing_path(std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header);
    PathData *get_the_highest_priority_path(std::vector<PathData *> &pitEntries);
    std::vector<PathData *> get_the_newly_measured_paths(std::vector<PathData *> &pitEntries);
    PathData *get_the_oldest_measured_path(std::vector<PathData *> &pitEntries);
    uint32_t get_the_path_length_by_path_id(const uint32_t pathId, PathData *&pitEntry);
    std::vector<PathData *> get_the_piggyback_pit_entries(uint32_t srcToRId, uint32_t dstHostId);
    uint32_t get_the_potential_paths(PathData *bestPitEntry, std::vector<PathData *> &allPitEntries, std::vector<PathData *> &potentialPitEntries);
    uint32_t get_the_probe_paths(std::vector<PathData *> &expiredPitEntries, std::vector<PathData *> &probePitEntries);
    PathData *get_the_random_path(std::vector<PathData *> &pitEntries);
    // PathData *get_the_round_robin_path_for_packet(pstEntryData *pstEntry, std::vector<PathData *> &pitEntries);
    // PathData *get_the_round_robin_path_for_flow(pstEntryData *pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header);
    // PathData *get_the_round_robin_path_for_hybird(pstEntryData *pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header);
    // PathData *get_the_least_congested_path(pstEntryData *pstEntry, std::vector<PathData *> &pitEntries, Ptr<const Packet> packet, const Ipv4Header &header);

    // uint32_t get_the_routing_index();

    PathData *get_the_smallest_latency_path(std::vector<PathData *> &pitEntries);
    std::map<Ipv4Address, hostIp2SMT_entry_t> get_SMT() const;
    PathData *get_the_best_probing_path(std::vector<PathData *> &pitEntries);
    void initialize();
    uint32_t install_PIT(std::map<uint32_t, PathData> &pit);
    uint32_t install_PST(std::map<HostId2PathSeleKey, pstEntryData> &pst);
    uint32_t install_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &SMT);
    PathData *lookup_PIT(uint32_t pieKey);
    pstEntryData *lookup_PST(HostId2PathSeleKey &pstKey);
    pdt_entry_t *lookup_PDT(HostId2PathSeleKey &pstKey);
    hostIp2SMT_entry_t *lookup_SMT(const Ipv4Address &serverAddr);
    uint32_t print_PIT();
    // uint32_t print_PST();
    uint32_t print_SMT();
    std::string ipv4Address2string(Ipv4Address addr);
    std::string construct_target_string_strlen(uint32_t strLen, std::string c);
    bool output_packet_by_path_tag(Ptr<Packet> packet, CustomHeader &ch, uint32_t pg);

    bool reach_the_last_hop_of_path_tag(Ipv4SmartFlowPathTag &smartFlowTag, PathData *&pitEntry);

    void receive_normal_packet(Ptr<Packet> &pkt, Ipv4SmartFlowPathTag &pathTag, PathData *&pitEntry);
    void receive_probe_packet(Ipv4SmartFlowProbeTag &probeTag);
    void record_the_probing_info(uint32_t pathId);
    Ptr<Packet> reply_probe_info(Ptr<Packet> &p, CustomHeader &ch);
    // void record_reorder_at_dst_tor(Ptr<Packet> &pkt, const Ipv4Header &header);

    void set_max_piggyback_path_number(uint32_t piggyLatencyCnt);
    void set_path_expire_interval(uint32_t a);
    void set_PIT(std::map<uint32_t, PathData> &nexthopSelTbl);
    void set_probing_interval(uint32_t probeTimeInterval);
    void set_PST(std::map<HostId2PathSeleKey, pstEntryData> &pathSelTbl);
    void set_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &vmVtepMapTbl);
    void update_path_tag(Ptr<Packet> &packet, Ipv4SmartFlowPathTag &smartFlowTag);
    uint32_t update_PIT_after_piggybacking(std::vector<PathData *> &piggyBackPitEntries);
    void update_PIT_after_probing(PathData *pitEntry);
    void update_PIT_by_latency_data(LatencyData &latencyData);
    void update_PIT_by_latency_tag(Ptr<Packet> &packet);
    void update_PIT_by_path_tag(Ipv4SmartFlowPathTag &pathTag, PathData *&pitEntry);
    void update_PIT_by_probe_tag(Ipv4SmartFlowProbeTag &probeTag);
    void update_PIT_after_adding_path_tag(PathData *forwardPitEntry);
    void update_PST_after_adding_path_tag(pstEntryData *pstEntry, PathData *forwardPitEntry);
    bool insert_entry_to_PIT(PathData &pitEntry);
    bool insert_entry_to_PST(pstEntryData &pstEntry);
    bool insert_entry_to_SMT(hostIp2SMT_entry_t &smtEntry);

    std::vector<double> CalPathWeightBasedOnDelay(const std::vector<PathData *> paths);
    uint32_t GetPathBasedOnWeight(const std::vector<double> & weights);


    // void add_conga_tag_by_pit_entry(Ptr<Packet> packet, PathData *pitEntry);
    // void update_PIT_by_conga_tag(Ipv4SmartFlowCongaTag &congaTag);
    std::vector<uint32_t> get_dre_of_egress_ports(std::vector<uint32_t> &ports);
    // uint32_t QuantizingX(uint32_t X);
    // uint32_t UpdateLocalDre(const Ipv4Header &header, Ptr<Packet> packet, uint32_t port);

    void SetSwitchInfo(bool isToR, uint32_t switch_id);
    void SetNode(Ptr<Node> node);
    bool IsE2ELb(void);

    // void DreEvent();
    // void AgingEvent();
    // void PrintDreTable();
    /*-----CALLBACK------*/
    void DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev,
                      uint32_t qIndex);                      // TxToR and Agg/CoreSw
    void DoSwitchSendToDev(Ptr<Packet> p, CustomHeader &ch); // only at RxToR
    typedef Callback<void, Ptr<Packet>, CustomHeader &, uint32_t, uint32_t> SwitchSendCallback;
    typedef Callback<void, Ptr<Packet>, CustomHeader &> SwitchSendToDevCallback;
    void SetSwitchSendCallback(SwitchSendCallback switchSendCallback);                // set callback
    void SetSwitchSendToDevCallback(SwitchSendToDevCallback switchSendToDevCallback); // set callback

    void RouteOutputForAckPktOnSrcHostForLaps(Ptr<E2ESrcOutPackets> entry);
    void RouteOutputForDataPktOnSrcHostForLaps(Ptr<E2ESrcOutPackets> entry);
    PathData * CheckProbePathAmoungPitEntries(std::vector<PathData *> & pitEntries);


  private:
    // callback
    SwitchSendCallback m_switchSendCallback;           // bound to SwitchNode::SwitchSend (for Request/UDP)
    SwitchSendToDevCallback m_switchSendToDevCallback; // bound to SwitchNode::SendToDevContinue (for Probe, Reply)
    // Ipv4 associated with this router
    // Ptr<Ipv4> m_ipv4;

    std::map<HostId2PathSeleKey, pstEntryData> m_pathSelTbl;
    std::map<std::string, flet_entry_t> m_fletTbl;
    std::map<std::string, rbn_entry_t> m_rbnTbl;
    std::map<uint32_t, PathData> m_nexthopSelTbl;
    std::map<Ipv4Address, hostIp2SMT_entry_t> m_vmVtepMapTbl;
    std::map<HostId2PathSeleKey, pdt_entry_t> m_pathDecTbl;

    uint32_t m_nodeId;
    uint32_t m_switch_id;
    Ptr<Node> m_node;
    uint32_t m_probeStrategy;
    uint32_t m_pathExpiredTimeThld;
    uint32_t m_pathSelNum;
    uint32_t m_pathSelStrategy; //
    uint32_t m_piggybackStrategy;
    uint32_t m_flowletTimoutInNs;
    bool m_enableFeedbackAllPathInfo;
    bool m_enabledAllPacketFeedbackInfo;
    bool m_isToR;

    bool lb_isInstallSever;
    EventId m_dreEvent;
    // Metric aging event
    EventId m_agingEvent;

    // Parameters
    // DRE
    std::map<uint32_t, uint32_t> m_XMap;
    double m_alpha;
    uint32_t m_Q;
    // DataRate m_C;
    Time m_agingTime;
    Time m_tdre;
    uint32_t m_probeTimeInterval;
    uint32_t m_piggyLatencyCnt;
    uint32_t m_reorderFlag;

    // Quantizing bits
  };

}

#endif /* RDMA_SMARTFLOW_ROUTING_H */
