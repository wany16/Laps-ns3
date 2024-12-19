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
 * Author: Valerio Sartini (valesar@gmail.com)
 */

#ifndef DCN_TOPOLOGY_READER_H
#define DCN_TOPOLOGY_READER_H

#include "topology-reader.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/names.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/attribute-helper.h"
#include "ns3/attribute.h"
#include <ns3/string.h>
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-l3-protocol.h"
#include <iomanip>
#include "ns3/channel.h"
#include "ns3/data-rate.h"
#include "ns3/queue.h"



/**
 * \file
 * \ingroup topology
 * ns3::DcnTopologyReader declaration.
 */

namespace ns3 {

  /**
   * \brief parser the attribute of node or channel
   * \return the map between the attributes' names and values.
   */
std::map<std::string, std::string> AttributeParse(std::string str, const char split);
std::string Ipv4Address2String(Ipv4Address addr) ;


// ------------------------------------------------------------
// --------------------------------------------
/**
 * \ingroup topology
 *
 * \brief Topology file reader (Dcn-format type).
 *
 * This class takes an input file in Inet format and extracts all
 * the information needed to build the topology
 * (i.e.number of nodes, links and links structure).
 * It have been tested with Inet 3.0
 * http://topology.eecs.umich.edu/inet/
 *
 * It might set a link attribute named "Weight", corresponding to
 * the euclidean distance between two nodes, the nodes being randomly positioned.
 */
class DcnTopologyReader : public TopologyReader {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId.
   */
  static TypeId GetTypeId (void);
  DcnTopologyReader ();
  virtual ~DcnTopologyReader ();

  /**
   * \brief Main topology reading function.
   *
   * This method opens an input stream and reads the Inet-format file.
   * From the first line it takes the total number of nodes and links.
   * Then discards a number of rows equals to total nodes (containing
   * useless geographical information).
   * Then reads until the end of the file (total links number rows) and saves
   * the structure of every single link in the topology.
   *
   * \return The container of the nodes created (or empty container if there was an error)
   */
  virtual NodeContainer Read (void);
  void InstallChannels();
  void AssignAddresses();
  void PrintNodeConnections (Ptr<Node> node, Ptr<OutputStreamWrapper> stream);
  void PrintAllConnections (Ptr<OutputStreamWrapper> stream);
  void PrintNodeIpv4Addresses (Ptr<Node> node, Ptr<OutputStreamWrapper> stream);
  void PrintAllIpv4Addresses (Ptr<OutputStreamWrapper> stream);
private:
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse.
   */
  DcnTopologyReader (const DcnTopologyReader&);
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse.
   * \returns
   */
  DcnTopologyReader& operator= (const DcnTopologyReader&);

  NodeContainer m_nodes; // nodes.
  std::vector<std::map<std::string, std::string> > m_channels; // channels
  std::map<Ptr<Node>, NetDeviceContainer> m_devices; // channels

  // end class DcnTopologyReader
};

// end namespace ns3
};


#endif /* DCN_TOPOLOGY_READER_H */
