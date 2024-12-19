/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Universita' di Firenze, Italy
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
 * Author: Tommaso Pecorella (tommaso.pecorella@unifi.it)
 * Author: Valerio Sartini (Valesar@gmail.com)
 */

#include <fstream>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include "ns3/node-container.h"
#include "ns3/log.h"

#include "dcn-topology-reader.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/drop-tail-queue.h"


/**
 * \file
 * \ingroup topology
 * ns3::DcnTopologyReader implementation.
 */

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DcnTopologyReader");

NS_OBJECT_ENSURE_REGISTERED (DcnTopologyReader);

std::map<std::string, std::string> AttributeParse(std::string str, const char split) {
  std::map<std::string, std::string> attributes;
  std::vector<std::string> res;
	std::istringstream iss(str);	// 输入流
	std::string token;			// 接收缓冲区
	while (getline(iss, token, split)){	// 以split为分隔符
    token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
		res.push_back(token);
    // std::cout << res.size() << ": " << token << std::endl;
	}
  if (res.size() % 2 != 0) {
      NS_LOG_ERROR ("Unexpected Node/Channel Attribute input: " << res.size());
      return attributes;
  }
  for (uint32_t i = 0; i < res.size(); i=i+2) {
    attributes[res[i]] = res[i+1];
  }
  return attributes;
}

std::string Ipv4Address2String(Ipv4Address addr) {
  std::ostringstream os;
  addr.Print(os);
  std::string addrStr = os.str();
  return addrStr;
}

TypeId DcnTopologyReader::GetTypeId (void)
{
        std::cout << "DcnTopologyReader::GetTypeId (void)" << std::endl;

  static TypeId tid = TypeId ("ns3::DcnTopologyReader")
    .SetParent<TopologyReader> ()
    .SetGroupName ("TopologyReader")
    .AddConstructor<DcnTopologyReader> ()
  ;
  return tid;
}

DcnTopologyReader::DcnTopologyReader ()
{
      std::cout << "DcnTopologyReader::DcnTopologyReader ()" << std::endl;

  NS_LOG_FUNCTION (this);
}

DcnTopologyReader::~DcnTopologyReader ()
{
  NS_LOG_FUNCTION (this);
}

NodeContainer DcnTopologyReader::Read (void) {
  std::ifstream topgen;
  topgen.open (GetFileName ().c_str ());
  if ( !topgen.is_open () ) {
      NS_LOG_WARN ("Dcn topology file " << GetFileName() << " object is not open, check file name and permissions");
      return m_nodes;
    }


  std::istringstream lineBuffer;
  std::string line;

  int totnode = 0;
  getline (topgen,line);
  lineBuffer.clear ();
  lineBuffer.str (line);
  lineBuffer >> totnode;
  NS_LOG_INFO ("Dcn topology should have " << totnode << " nodes ");
  for (int i = 0; i < totnode && !topgen.eof (); i++) {
      getline (topgen,line);
      std::map<std::string, std::string> attributes = AttributeParse(line, ' ');
      auto it_type = attributes.find("Type");
      if (it_type != attributes.end()) {
        std::cout << "it_type->second: " << it_type->second << "* *"<< std::endl;
         if(it_type->second == "Node"){
              Ptr<Node> tmpNode = CreateObject<Node> ();
              m_nodes.Add(tmpNode);
              auto it_name = attributes.find("Name");
              if (it_name != attributes.end()) {
                Names::Add(it_name->second, tmpNode);
                std::cout << "Name: " << it_name->second << ", NodeID: " << tmpNode->GetId() << std::endl;
              }else{
                Names::Add("Node"+std::to_string(i+1), tmpNode);
              }
          }else{
            NS_LOG_ERROR ("Unexpected Node Type: " << it_type->second);
          }          
      }
  }

  int totlink = 0;
  getline (topgen,line);
  lineBuffer.clear ();
  lineBuffer.str (line);
  lineBuffer >> totlink;
  NS_LOG_INFO ("Dcn topology should have " << totlink << " links");

  for (int i = 0; i < totlink && !topgen.eof (); i++) {
      getline (topgen,line);
      std::map<std::string, std::string> attributes = AttributeParse(line, ' ');
      m_channels.push_back(attributes);
    }

  NS_LOG_INFO ("Dcn topology Read with " << m_nodes.GetN() << " nodes and " << m_channels.size() << " links");
  topgen.close ();

  return m_nodes;
}

  void DcnTopologyReader::InstallChannels() {
    for (uint32_t i = 0; i < m_channels.size(); i++) {
      auto chl = m_channels[i];
      Ptr<Node> fromNode = Names::Find<Node> (chl["From"]);
      Ptr<Node> toNode = Names::Find<Node> (chl["To"]);
      std::string type = chl["Type"];
      if (type == "P2P")  {
          PointToPointHelper p2p;
          p2p.SetDeviceAttribute("DataRate", StringValue(chl["Rate"]));
          p2p.SetChannelAttribute("Delay", StringValue(chl["Delay"]));
          p2p.SetQueue("ns3::DropTailQueue"); // 队列缓存大小
          NetDeviceContainer devs = p2p.Install(fromNode, toNode);
          m_devices[fromNode].Add(devs.Get(0));
          m_devices[toNode].Add(devs.Get(1));
          Names::Add(chl["From"] + ":" + std::to_string(m_devices[fromNode].GetN()+1) + "-" +
                     std::to_string(m_devices[toNode].GetN()+1) + ":" + chl["To"], devs.Get(0));

          Names::Add(chl["To"] + ":" + std::to_string(m_devices[toNode].GetN()+1) + "-" +
                     std::to_string(m_devices[fromNode].GetN()+1) + ":" + chl["From"], devs.Get(1));
        }
        else{
          NS_LOG_ERROR ("Unknwon channel type: " << type);
        }
      
    }
    return ;
  }

  void DcnTopologyReader::AssignAddresses() {
    for (auto & it : m_devices) {
      auto tmpNode = it.first;
      auto devs = it.second;
      Ipv4Mask msk("255.255.255.0");
      Ipv4Address base = Ipv4Address("0.0.0.1");
      Ipv4Address ntwk(0x0b000000 + ((tmpNode->GetId() / 256) * 0x00010000) + ((tmpNode->GetId() % 256) * 0x00000100));
      Ipv4AddressHelper hlpr;
      hlpr.SetBase(ntwk, msk, base);
      hlpr.Assign(devs);
    }
    return ;
  }

void DcnTopologyReader::PrintNodeConnections (Ptr<Node> node, Ptr<OutputStreamWrapper> stream) {
  
  if (node == 0) {
    NS_LOG_ERROR ("UnExisted Node");
    return ;
  }

  std::string nodeName = Names::FindName (node);
  if (nodeName == "") {
    NS_LOG_ERROR ("Unknwon Name for Node " << node->GetId());
    nodeName = "Unknown";
  }
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();

  if (ipv4 == 0)  {
    NS_LOG_WARN ("Unexist Ipv4 Object For Node " << node->GetId());
    return ;
  }
  

  std::ostream* os = stream->GetStream ();
  *os << "###################";
  *os << "Connections of node: " << nodeName << "(" << node->GetId() << ")";
  *os << " at time " << Now ().GetMicroSeconds () << "us";
  *os << "###################";
  *os << "\n";
  uint32_t nicCnt = node->GetNDevices ();
  if (nicCnt == 1) {
    *os << " No user installed connections except the loopback one" << "\n";
    return ;
  }
  *os << std::setiosflags (std::ios::left) << std::setw (16) << "SelfIndex";
  *os << std::setiosflags (std::ios::left) << std::setw (16) << "OtherNode";
  *os << std::setiosflags (std::ios::left) << std::setw (16) << "OtherIndex";
  *os << std::setiosflags (std::ios::left) << std::setw (16) << "Type";
  *os << std::setiosflags (std::ios::left) << std::setw (16) << "Rate";
  *os << std::setiosflags (std::ios::left) << std::setw (16) << "Delay";
  *os << std::setiosflags (std::ios::left) << std::setw (20) << "QueueType";
  *os << std::setiosflags (std::ios::left) << std::setw (10) << "QueueSize";
  *os << "\n";


  for (uint32_t j = 1; j < nicCnt; ++j) {
    *os << std::setiosflags (std::ios::left) << std::setw (16) << j;
    Ptr<NetDevice> dev = node->GetDevice (j); // get the netdevice linked to the port-th port/netdevice
    Ptr<Channel> channel = dev->GetChannel (); // get the channel linked to the port-th port
    uint32_t otherEnd = (channel->GetDevice (0) == dev) ? 1 : 0; // 查看对端是这个channel的0或1
    Ptr<Node> otherNode = channel->GetDevice (otherEnd)->GetNode (); // 找到下一跳节点node
    std::string otherNodeName = Names::FindName (otherNode);
    *os << std::setiosflags (std::ios::left) << std::setw (16) << otherNodeName;
    uint32_t otherIndex = channel->GetDevice (otherEnd)->GetIfIndex (); // 查看是对端node的第几个netdevice
    *os << std::setiosflags (std::ios::left) << std::setw (16) << otherIndex;
    Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice>(dev);
    if (p2pDev == 0) {
      *os << std::setiosflags (std::ios::left) << std::setw (16) << "Un-P2P" << "\n";
      continue;
    }else{
      *os << std::setiosflags (std::ios::left) << std::setw (16) << "P2P";
    }
    uint64_t rateInGbps = p2pDev->GetDataRate().GetBitRate()/1000000000; // get the netdevice linked to the port-th port/netdevice
    *os << std::setiosflags (std::ios::left) << std::setw (16) << std::to_string(rateInGbps)+"Gbps";
    Ptr<PointToPointChannel> p2pChannel = DynamicCast<PointToPointChannel>(channel);

    uint64_t delayInUs = p2pChannel->GetDelay().GetNanoSeconds()/1000; // get the netdevice linked to the port-th port/netdevice
    *os << std::setiosflags (std::ios::left) << std::setw (16) << std::to_string(delayInUs)+"us";
    Ptr<Queue<Packet> > queue = p2pDev->GetQueue ();
    Ptr<DropTailQueue<Packet> > dropTaiQueue = DynamicCast<DropTailQueue<Packet> >(queue);
    if (dropTaiQueue == 0) {
      *os << std::setiosflags (std::ios::left) << std::setw (20) << "Un-Droptail";
    }else{
      *os << std::setiosflags (std::ios::left) << std::setw (20) << "Droptail";

    }
    
    // *os << std::setiosflags (std::ios::left) << std::setw (20) << queueTypeName;
    uint32_t queueSize = queue->GetMaxSize().GetValue();
    std::string queueUnit = queue->GetMaxSize().GetUnit() == QueueSizeUnit::PACKETS ? "Packets" : "Bytes";
    *os << std::setiosflags (std::ios::left) << std::setw (10) << std::to_string(queueSize)+queueUnit;
    *os << "\n";
    }

    
}

void DcnTopologyReader::PrintAllConnections (Ptr<OutputStreamWrapper> stream) {
  std::ostream* os = stream->GetStream ();
  *os << "******************************************************";
  *os << "Print Connections of total " << m_nodes.GetN() << " nodes ";
  *os << "at time " << Now ().GetMicroSeconds () << "us";
  *os << "******************************************************";
  *os << "\n";
  for (uint32_t i = 0; i < m_nodes.GetN(); i++) {
    PrintNodeConnections (m_nodes.Get(i), stream);
  }
}

void DcnTopologyReader::PrintNodeIpv4Addresses (Ptr<Node> node, Ptr<OutputStreamWrapper> stream) {
  
  if (node == 0) {
    NS_LOG_ERROR ("UnExisted Node");
    return ;
  }

  std::string nodeName = Names::FindName (node);
  if (nodeName == "") {
    NS_LOG_ERROR ("Unknwon Name for Node " << node->GetId());
    nodeName = "Unknown";
  }
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();

  if (ipv4 == 0)  {
    NS_LOG_WARN ("Unexist Ipv4 Object For Node " << node->GetId());
    return ;
  }
  

  std::ostream* os = stream->GetStream ();
  *os << "###################";
  *os << "Ipv4Addresses of node: " << nodeName << "(" << node->GetId() << ")";
  *os << " at time " << Now ().GetMicroSeconds () << "us";
  *os << "###################";
  *os << "\n";
  uint32_t nicCnt = ipv4->GetNInterfaces ();
  if (nicCnt == 1) {
    *os << " No user installed connections except the loopback one" << "\n";
    return ;
  }
  *os << std::setiosflags (std::ios::left) << std::setw (10) << "Index";
  *os << std::setiosflags (std::ios::left) << std::setw (20) << "Address";
  *os << "\n";


  for (uint32_t j = 1; j < nicCnt; ++j) {
    *os << std::setiosflags (std::ios::left) << std::setw (10) << j;
    Ipv4Address curAddr = ipv4->GetAddress (j,0).GetLocal();
    *os << std::setiosflags (std::ios::left) << std::setw (20) << Ipv4Address2String(curAddr);
    *os << "\n";
    }   
}

void DcnTopologyReader::PrintAllIpv4Addresses (Ptr<OutputStreamWrapper> stream) {
  std::ostream* os = stream->GetStream ();
  *os << "******************************************************";
  *os << "Print Addresses of total " << m_nodes.GetN() << " nodes ";
  *os << "at time " << Now ().GetMicroSeconds () << "us";
  *os << "******************************************************";
  *os << "\n";

  for (uint32_t i = 0; i < m_nodes.GetN(); i++) {
    PrintNodeIpv4Addresses (m_nodes.Get(i), stream);
  }
}


} /* namespace ns3 */
