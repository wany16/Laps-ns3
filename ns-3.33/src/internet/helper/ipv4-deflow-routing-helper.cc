/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ipv4-deflow-routing-helper.h"
#include <ns3/string.h>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <set>
#include "ns3/output-stream-wrapper.h"

namespace ns3 {



/* add by ying on March 29, 2023 */


// NS_LOG_COMPONENT_DEFINE ("Ipv4DeflowRoutingHelper");
// NS_OBJECT_ENSURE_REGISTERED (Ipv4DeflowRoutingHelper);

std::map<Ptr<Node>, bool> Ipv4DeflowRoutingHelper::installedNodes;
std::map<Ptr<Node>, bool > Ipv4DeflowRoutingHelper::reachableNodes;
std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<uint32_t> > > Ipv4DeflowRoutingHelper::nodesGraph;
std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<NodePath> > > Ipv4DeflowRoutingHelper::nodesPaths;
std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<PortPath> > > Ipv4DeflowRoutingHelper::portsPaths; 

uint32_t Ipv4DeflowRoutingHelper::nodesPathNum = 0;
uint32_t Ipv4DeflowRoutingHelper::portsPathNum = 0;


void Ipv4DeflowRoutingHelper::DiscoverTopologyNodes(){
  reachableNodes.clear();
  for (auto &it_0 : installedNodes) {
    auto tmpNode = it_0.first;
    if (reachableNodes.find(tmpNode) == reachableNodes.end()) {
      std::queue<Ptr<Node> > q;
      q.push(tmpNode);
      reachableNodes[tmpNode] = true;
      while (!q.empty()) {
        Ptr<Node> visitingNode = q.front();
        uint32_t devCnt = visitingNode->GetNDevices();
        for (uint32_t devIdx = 1; devIdx < devCnt; devIdx++) {
            Ptr<NetDevice> tmpDev = visitingNode->GetDevice(devIdx);
            Ptr<Channel> tmpChl = tmpDev->GetChannel (); // get the channel linked to the port-th port
            uint32_t otherEnd = (tmpChl->GetDevice (0) == tmpDev) ? 1 : 0; // 查看对端是这个channel的0或1
            Ptr<Node> adjNode = tmpChl->GetDevice (otherEnd)->GetNode (); // 找到下一跳节点node
            if (reachableNodes.find(adjNode) == reachableNodes.end()) {
              q.push(adjNode);
              reachableNodes[adjNode] = true;
            }
        }
        q.pop();
      }
    }
  }
}

void Ipv4DeflowRoutingHelper::BuildingTopologyGraph(){
  nodesGraph.clear();
  for (auto & it_0 : reachableNodes)  {
    auto tmpNode = it_0.first;
    uint32_t devCnt = tmpNode->GetNDevices();
    for (uint32_t devIdx = 1; devIdx < devCnt; devIdx++) {
        Ptr<NetDevice> tmpDev = tmpNode->GetDevice(devIdx);
        Ptr<Channel> tmpChl = tmpDev->GetChannel (); // get the channel linked to the port-th port
        uint32_t otherEnd = (tmpChl->GetDevice (0) == tmpDev) ? 1 : 0; // 查看对端是这个channel的0或1
        Ptr<Node> adjNode = tmpChl->GetDevice (otherEnd)->GetNode (); // 找到下一跳节点node
        nodesGraph[tmpNode][adjNode].push_back(devIdx);
    }
  }
  

}

void Ipv4DeflowRoutingHelper::PrintTopologyGraph(Ptr<OutputStreamWrapper> stream){

  std::ostream* os = stream->GetStream ();
  *os << "******************************************************";
  *os << "Print Topology Graph of total " << reachableNodes.size() << " discovered nodes ";
  *os << "at time " << Now ().GetMicroSeconds () << "us";
  *os << "******************************************************";
  *os << "\n";

  *os << std::setiosflags (std::ios::left) << std::setw (10) << "Index";


  for (auto & it_0 : nodesGraph)  {
    auto tmpNode = it_0.first;
    std::string nodeName = Names::FindName (tmpNode);
    if (nodeName == "") {
      nodeName = "Unknown(" + std::to_string(tmpNode->GetId()) + ")";
    }
    *os << "###################";
    *os << "Node: " << nodeName;
    *os << " ###################";
    *os << "\n";
  

    *os << std::setiosflags (std::ios::left) << std::setw (10) << "Index";
    *os << std::setiosflags (std::ios::left) << std::setw (15) << "Neighbor";
    *os << std::setiosflags (std::ios::left) << std::setw (40) << "EgressPorts";
    *os << "\n";

    uint32_t adjIndex = 1;
    for (auto & it_1 : it_0.second) {
        Ptr<Node> adjNode = it_1.first;
        *os << std::setiosflags (std::ios::left) << std::setw (10) << adjIndex;
        nodeName = Names::FindName (adjNode);
        if (nodeName == "") {
          nodeName = "Unknown(" + std::to_string(adjNode->GetId()) + ")";
        }
        *os << std::setiosflags (std::ios::left) << std::setw (15) << nodeName;

        *os << std::setiosflags (std::ios::left) << std::setw (40) << vector2string<uint32_t>(it_1.second, ", ");
        *os << "\n";
        adjIndex += 1;
    }
  }
}




void Ipv4DeflowRoutingHelper::InitializeNodesGraph(){
  Ipv4DeflowRoutingHelper::DiscoverTopologyNodes();
  Ipv4DeflowRoutingHelper::BuildingTopologyGraph();
}

// DFS 函数
void Ipv4DeflowRoutingHelper::FindShortestPaths(Ptr<Node> srcNode, Ptr<Node> dstNode, std::vector<Ptr<Node> >& tmpPath,
                  std::map<Ptr<Node>, bool>& visited, std::vector<std::vector<Ptr<Node> > >& allPaths) {
    // 将当前节点加入路径
    tmpPath.push_back(srcNode);
    visited[srcNode] = true;

    // 如果到达目标节点，保存路径
    if (srcNode == dstNode) {
        uint32_t minPathLen = allPaths.size()==0 ? std::numeric_limits<uint32_t>::max () : allPaths[0].size();
        if (minPathLen == tmpPath.size()) {
          allPaths.push_back(tmpPath);
        }
        else if (minPathLen > tmpPath.size()) {
          allPaths.clear();
          allPaths.push_back(tmpPath);
        }
    } else {
        uint32_t minPathLen = allPaths.size()==0 ? std::numeric_limits<uint32_t>::max () : allPaths[0].size();
        if (minPathLen > tmpPath.size()) {
          // 否则，继续遍历相邻节点
          for (auto& it_node : nodesGraph[srcNode]) {
              auto adjNode = it_node.first;
              // 避免环路
              if ((visited.find(adjNode) == visited.end()) ||  (visited[adjNode] == false)) {
                  FindShortestPaths(adjNode, dstNode, tmpPath, visited, allPaths);
              }
          }
        }
    }
    // 回溯：移除当前节点
    tmpPath.pop_back();
    visited[srcNode] = false;
}

void Ipv4DeflowRoutingHelper::CalculateNodePaths(){
  for (auto & it_0 : installedNodes) {
    auto srcNode = it_0.first;
    for (auto & it_1 : reachableNodes) {
      auto dstNode = it_1.first;

      if (srcNode != dstNode) {
        std::vector<Ptr<Node> > tmpPath;
        std::map<Ptr<Node>, bool> visited;
        std::vector<std::vector<Ptr<Node> > > allPaths;
        FindShortestPaths(srcNode, dstNode, tmpPath, visited, allPaths);
        for (auto &nodes : allPaths) {
          nodesPathNum += 1;
          NodePath * p = new NodePath(nodes, nodesPathNum);
          nodesPaths[srcNode][dstNode].push_back(*p);
        }
      }
    }
  }
}

void Ipv4DeflowRoutingHelper::CalculatePortPaths(){
  for (auto & it_0 : nodesPaths) {
    auto srcNode = it_0.first;
    // std::string srcNodeName = GetNodeName(srcNode);
    for (auto & it_1 : it_0.second) {
      
      auto dstNode = it_1.first;
      // std::cout << "srcNode: " << srcNodeName << " --> ";
      // std::string dstNodeName = GetNodeName(dstNode);
      // std::cout << "DstNode: " << dstNodeName << std::endl;

      for (auto & tmpNodePath : it_1.second) {
        std::vector<std::vector<uint32_t> > ports;
        for (int32_t i = 0; i < tmpNodePath.nodes.size()-1; i++) {
          Ptr<Node> curNode = tmpNodePath.nodes[i];
          Ptr<Node> nextNode = tmpNodePath.nodes[i+1];
          std::vector<uint32_t> tmpPorts = nodesGraph[curNode][nextNode];
          ports.push_back(tmpPorts);
          //  std::cout << "Ports: " << vector2string(tmpPorts) << std::endl;
        }

        if (!ports.size())  {
          std::cout << "Error, No Exist NodePath" << std::endl;
          std::string srcNodeName = GetNodeName(srcNode);
          std::cout << "srcNode: " << srcNodeName << " --> ";
          std::string dstNodeName = GetNodeName(dstNode);
          std::cout << "DstNode: " << dstNodeName << "\n";
          return ;
        }

        std::vector<uint32_t> stateStack(1, 0);
        std::vector<uint32_t> tmpPorts(1, ports[0][0]);

        while(stateStack.size()){

          while (stateStack.size() < ports.size()) {
            tmpPorts.push_back(ports[stateStack.size()][0]);
            stateStack.push_back(0);
            // std::cout << "Forward" << std::endl;
            // std::cout << "stateStack: " << vector2string<uint32_t> (stateStack) << std::endl;
            // std::cout << "tmpPorts: " << vector2string<uint32_t> (tmpPorts) << std::endl;
          }


          PortPath * tmpPortPath = new PortPath(tmpPorts, portsPathNum++);
          portsPaths[srcNode][dstNode].push_back(*tmpPortPath);

          while ( (stateStack.size()>0) && (stateStack.back() + 1 == ports[stateStack.size()-1].size())) {
            stateStack.pop_back();
            tmpPorts.pop_back();
            // std::cout << "backtrack" << std::endl;
            // std::cout << "stateStack: " << vector2string<uint32_t> (stateStack) << std::endl;
            // std::cout << "tmpPorts: " << vector2string<uint32_t> (tmpPorts) << std::endl;
          }

          if (stateStack.size()) {
            tmpPorts.pop_back();
            uint32_t nextIdx = stateStack.back()+1;
            tmpPorts.push_back(ports[stateStack.size()-1][nextIdx]);
            stateStack.pop_back();
            stateStack.push_back(nextIdx);
          }

          }


      }

      

      }
    }
  }



void Ipv4DeflowRoutingHelper::PrintNodePaths(Ptr<OutputStreamWrapper> stream){

  std::ostream* os = stream->GetStream ();
  *os << "******************************************************";
  *os << "All Node-based Paths ";
  *os << "at time " << Now ().GetMicroSeconds () << "us";
  *os << "******************************************************";
  *os << "\n";

  *os << std::setiosflags (std::ios::left) << std::setw (10) << "Index";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "SrcNode";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "DstNode";
  *os << std::setiosflags (std::ios::left) << std::setw (60) << "InterNode";
  *os << "\n";

  for (auto & it_0 : nodesPaths) {
    for (auto & it_1 : it_0.second) {
      for (auto & it_2 : it_1.second) {
        it_2.Print(stream);
          *os << "\n";

      }
    }
  }
}

void Ipv4DeflowRoutingHelper::PrintPortPaths(Ptr<OutputStreamWrapper> stream){

  std::ostream* os = stream->GetStream ();
  *os << "******************************************************";
  *os << "All Port-based Paths ";
  *os << "at time " << Now ().GetMicroSeconds () << "us";
  *os << "******************************************************";
  *os << "\n";



  *os << std::setiosflags (std::ios::left) << std::setw (10) << "Index";
  *os << std::setiosflags (std::ios::left) << std::setw (10) << "PathId";
  *os << std::setiosflags (std::ios::left) << std::setw (60) << "Ports";
  *os << "\n";

    for (auto & it_0 : portsPaths) {
      auto srcNode = it_0.first;
      std::string srcNodeName = GetNodeName(srcNode);
      for (auto & it_1 : it_0.second) {
        *os << "-----------------srcNode: " << srcNodeName << " --> ";
        auto dstNode = it_1.first;
        std::string dstNodeName = GetNodeName(dstNode);
        *os << "DstNode: " << dstNodeName << "-----------------\n";
        uint32_t index = 0;
        for (auto & it_2 : it_1.second) {
          *os << std::setiosflags (std::ios::left) << std::setw (10) << index++;
          it_2.Print(stream);
          *os << "\n";
        }
      }
  }
}


void Ipv4DeflowRoutingHelper::PrintRoutingTables(Ptr<OutputStreamWrapper> stream){

  std::ostream* os = stream->GetStream ();
  *os << "******************************************************";
  *os << "Routing Tables of All Nodes that installing DeflowRouting Protocol ";
  *os << "at time " << Now ().GetMicroSeconds () << "us";
  *os << "******************************************************";
  *os << "\n";

    for (auto & it_0 : installedNodes) {
      auto tmpNode = it_0.first;
      Ptr<Ipv4DeflowRouting> rtp = GetRouting<Ipv4DeflowRouting>(tmpNode->GetObject<Ipv4>()->GetRoutingProtocol());
      if (rtp == 0) {
        NS_FATAL_ERROR ("Unable to find DeflowRouting on this Node ");
        return ;
      }
      rtp->PrintRoutingTable(stream);
  }
}







void Ipv4DeflowRoutingHelper::InstallRoutingEntries(){
  for (auto & it0 : installedNodes) {

    Ptr<Node> srcNode = it0.first;
    Ptr<Ipv4DeflowRouting> rtp = GetRouting<Ipv4DeflowRouting>(srcNode->GetObject<Ipv4>()->GetRoutingProtocol());
    if (rtp == 0) {
      NS_FATAL_ERROR ("Unable to find DeflowRouting on this Node ");
      return ;
    }
    
    for (auto & it1 : nodesPaths[srcNode]) {

      Ptr<Node> dstNode = it1.first;
      std::vector<Ipv4Address> ips;
      uint32_t nicCnt = dstNode->GetObject<Ipv4>()->GetNInterfaces ();
      for (uint32_t i = 1; i < nicCnt; i++) {
        Ipv4Address ip = dstNode->GetObject<Ipv4>()->GetAddress(i, 0).GetLocal();
        ips.push_back(ip);
      }

      std::set<uint32_t> egressPorts;
      for (auto & path : it1.second) {
        if (path.nodes.size() < 2) {
          NS_FATAL_ERROR ("Error Path Length");
          return ;
        }
        auto nextNode = path.nodes[1];
        std::vector<uint32_t> tmpEgressPorts = nodesGraph[srcNode][nextNode];
        std::copy(tmpEgressPorts.begin(), tmpEgressPorts.end(), std::inserter(egressPorts, egressPorts.end()));
      }

      for (auto & dip : ips)  {
        for (auto & port : egressPorts) {
          rtp->AddRouteEntry(dip, port);

        }
      }

    }
  }
}


void Ipv4DeflowRoutingHelper::CalculateRoutingTables(){
  Ipv4DeflowRoutingHelper::CalculateNodePaths();
  Ipv4DeflowRoutingHelper::CalculatePortPaths();

}


void Ipv4DeflowRoutingHelper::PopulateRoutingTables(){
     Ipv4DeflowRoutingHelper::InitializeNodesGraph();
     Ipv4DeflowRoutingHelper::CalculateRoutingTables();
     Ipv4DeflowRoutingHelper::InstallRoutingEntries();
}









Ipv4DeflowRoutingHelper::Ipv4DeflowRoutingHelper ()
{
    // std::cout << "Ipv4DeflowRoutingHelper::Ipv4DeflowRoutingHelper ()" << std::endl;

}

Ipv4DeflowRoutingHelper::Ipv4DeflowRoutingHelper (const Ipv4DeflowRoutingHelper& o) {
  // std::cout << "Ipv4DeflowRouting::Ipv4DeflowRouting ()" << std::endl;

}

Ipv4DeflowRoutingHelper* Ipv4DeflowRoutingHelper::Copy (void) const {
    // std::cout << "Ipv4DeflowRouting::Copy ()" << std::endl;

  return new Ipv4DeflowRoutingHelper (*this);
}

Ptr<Ipv4RoutingProtocol> Ipv4DeflowRoutingHelper::Create (Ptr<Node> node) const {
      // std::cout << "Ipv4DeflowRouting::Create ()" << std::endl;

  Ptr<Ipv4DeflowRouting> ipv4DeflowRouting = CreateObject<Ipv4DeflowRouting> ();
  installedNodes[node] = true;
  // std::cout << "installedNodes.size(): " << installedNodes.size() << std::endl;

  return ipv4DeflowRouting;
}




}

