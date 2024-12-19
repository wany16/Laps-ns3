/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 * Author: Valerio Sartini <valesar@gmail.com>
 *
 * This program conducts a simple experiment: It builds up a topology based on
 * either Inet or Orbis trace files. A random node is then chosen, and all the
 * other nodes will send a packet to it. The TTL is measured and reported as an histogram.
 *
 */

#include <ctime>

#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-nix-vector-helper.h"

#include "ns3/topology-read-module.h"
#include <list>

/**
 * \file
 * \ingroup topology
 * Example of TopologyReader: .read in a topology in a specificed format.
 */

//  Document the available input files
/**
 * \file RocketFuel_toposample_1239_weights.txt
 * Example TopologyReader input file in RocketFuel format;
 * to read this with topology-example-sim.cc use \c --format=Rocket
 */
/**
 * \file Inet_toposample.txt
 * Example TopologyReader input file in Inet format;
 * to read this with topology-example-sim.cc use \c --format=Inet
 */
/**
 * \file Inet_small_toposample.txt
 * Example TopologyReader input file in Inet format;
 * to read this with topology-example-sim.cc use \c --format=Inet
 */
/**
 * \file Orbis_toposample.txt
 * Example TopologyReader input file in Orbis format;
 * to read this with topology-example-sim.cc use \c --format=Orbis
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TopologyCreationExperiment");

static std::list<unsigned int> data;

static void SinkRx (Ptr<const Packet> p, const Address &ad)
{
  Ipv4Header ipv4;
  p->PeekHeader (ipv4);
  std::cout << "TTL: " << (unsigned)ipv4.GetTtl () << std::endl;
}


// ----------------------------------------------------------------------
// -- main
// ----------------------------------------------
int main (int argc, char *argv[])
{

  Config::SetDefault ("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, 100*1000)));

  std::string format ("dcn");
  std::string input ("src/topology-read/examples/topology-dcn-example.txt");

  // Set up command line parameters used to control the experiment.
  CommandLine cmd (__FILE__);
  cmd.AddValue ("format", "Format to use for data input [Orbis|Inet|Rocketfuel].",
                format);
  cmd.AddValue ("input", "Name of the input file.",
                input);
  cmd.Parse (argc, argv);


  // ------------------------------------------------------------
  // -- Read topology data.
  // --------------------------------------------

  // Pick a topology reader based in the requested format.
  NS_LOG_INFO ("Reading the topology File");
  TopologyReaderHelper topoHelp;
  topoHelp.SetFileName (input);
  topoHelp.SetFileType (format);
  Ptr<TopologyReader> topologyReader = topoHelp.GetTopologyReader ();

  NS_LOG_INFO ("Create the Nodes and Read the Channels");
  NodeContainer nodes = topologyReader->Read ();

  NS_LOG_INFO ("install internet stack and routing protocol");
  InternetStackHelper internetStackHelper;
  Ipv4DeflowRoutingHelper deflowRoutingHelper
  internetStackHelper.SetRoutingHelper (deflowRoutingHelper);  // has effect on the next Install ()
  internetStackHelper.Install (nodes);
  topologyReader->InstallChannels();
  topologyReader->AssignAddresses();
  Ipv4DeflowRoutingHelper::PopulateRoutingTables();


  Ptr<Node> srcNode = Names::Find<Ptr<Node> > ("Server1");
  Ptr<Node> dstNode = Names::Find<Ptr<Node> > ("Server2");

  install_tcp_test_applications (srcNode, dstNode); 

  // we trap the packet sink receiver to extract the TTL.
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&SinkRx));

  // ------------------------------------------------------------
  // -- Run the simulation
  // --------------------------------------------
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;

  // end main
}
