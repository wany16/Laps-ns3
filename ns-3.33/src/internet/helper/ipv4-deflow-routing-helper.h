/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef DEFLOW_HELPER_H
#define DEFLOW_HELPER_H

#include "ns3/ipv4-deflow-routing.h"
#include "ns3/log.h"
#include <unordered_set>

/* add by ying on July 03, 2024 */
#include "ns3/ipv4-routing-helper.h"


namespace ns3 {




struct PortPath {
  // std::string flowStr; 
  uint32_t pid;
  std::vector<uint32_t> ports;

  PortPath() {
    pid = 0;
    ports.clear();
  }

  PortPath(const std::vector<uint32_t> &n, const uint32_t p) {
    pid = p;
    ports = n;
  }

  void Print(Ptr<OutputStreamWrapper> stream){
    // pid src dst transit
    std::ostream* os = stream->GetStream ();
    if (ports.size() < 1) {
        *os << std::setiosflags (std::ios::left) << std::setw (10) << "Error";
        return ;
    }
    *os << std::setiosflags (std::ios::left) << std::setw (10) << pid;
    *os << std::setiosflags (std::ios::left) << std::setw (1) << "[";
    for (uint32_t i = 0; i < ports.size()-1; i++) {
      *os << ports[i] << ", ";
    }
    *os << ports[ports.size()-1] << "]";
  }
};

struct NodePath {
  // std::string flowStr; 
  uint32_t pid;
  std::vector<Ptr<Node> > nodes;

  void Print(Ptr<OutputStreamWrapper> stream){
    // pid src dst transit
    std::ostream* os = stream->GetStream ();
    if (nodes.size() < 2) {
        *os << std::setiosflags (std::ios::left) << std::setw (10) << "Error";
        return ;
    }
    *os << std::setiosflags (std::ios::left) << std::setw (10) << pid;
    std::string srcNodeName = GetNodeName(nodes[0]);
    *os << std::setiosflags (std::ios::left) << std::setw (12) << srcNodeName;

    std::string dstNodeName = GetNodeName(nodes[nodes.size()-1]); 
    *os << std::setiosflags (std::ios::left) << std::setw (12) << dstNodeName;

    *os << std::setiosflags (std::ios::left) << std::setw (1) << "[";
    for (uint32_t i = 1; i < nodes.size()-2; i++) {
      std::string tmpStr = GetNodeName(nodes[i]);
      *os << tmpStr;
      *os << " --> ";
    }
    if (nodes.size()-2 != 0) {
      std::string tmpStr = GetNodeName(nodes[nodes.size()-2]);
      *os << tmpStr;
    }else{
      *os << "null";
    }
    *os << "]";
  }

  NodePath() {
    pid = 0;
    nodes.clear();
  }

  NodePath(const std::vector<Ptr<Node> > &n, const uint32_t p) {
    pid = p;
    nodes = n;
  }

};





/* add by ying on March 29, 2023 */
class Ipv4DeflowRoutingHelper : public Ipv4RoutingHelper
{
public:

    Ipv4DeflowRoutingHelper ();
    Ipv4DeflowRoutingHelper (const Ipv4DeflowRoutingHelper&);
    static std::map<Ptr<Node>, bool> installedNodes;
    static std::map<Ptr<Node>, bool > reachableNodes;
    static std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<uint32_t> > > nodesGraph; // key: node, value: list of pairs (neighbor, edge_id)
    static std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<NodePath> > > nodesPaths; // key: node, value: list of pairs (neighbor, edge_id)
    static std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<PortPath> > > portsPaths; // key: node, value: list of pairs (neighbor, edge_id)
    static void PrintTopologyGraph(Ptr<OutputStreamWrapper> stream);
    static void PrintNodePaths(Ptr<OutputStreamWrapper> stream);
    static void PrintPortPaths(Ptr<OutputStreamWrapper> stream);
    static void PrintRoutingTables(Ptr<OutputStreamWrapper> stream);
    // static void PrintQueueLengths(Ptr<OutputStreamWrapper> stream, uint64_t intervalInNs, uint32_t round);

    static uint32_t nodesPathNum;
    static uint32_t portsPathNum;
    static void CalculatePortPaths();


    static void PopulateRoutingTables();
    static void InitializeNodesGraph();
    static void CalculateRoutingTables();
    static void InstallRoutingEntries();
    static void DiscoverTopologyNodes();
    static void BuildingTopologyGraph();
    static void CalculateNodePaths();
    static void FindShortestPaths(Ptr<Node> srcNode, Ptr<Node> dstNode, std::vector<Ptr<Node> >& tmpPath,
                  std::map<Ptr<Node>, bool>& visited, std::vector<std::vector<Ptr<Node> > >& allPaths);

    Ipv4DeflowRoutingHelper *Copy (void) const;
    virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;
};

}

#endif /* DRAG_FLOW_HELPER_H */

