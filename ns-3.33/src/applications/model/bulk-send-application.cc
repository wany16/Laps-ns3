/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Georgia Institute of Technology
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
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/boolean.h"
#include "bulk-send-application.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BulkSendApplication");

NS_OBJECT_ENSURE_REGISTERED (BulkSendApplication);

void tcpSocketBaseBxCb (std::string fid, std::map<std::string, reorderDistEntry> * reorderDistTbl, Ptr<const Packet> p, const TcpHeader& tcpHdr) {

      uint32_t seq = tcpHdr.GetSequenceNumber ().GetValue();
      uint32_t pktSieInByte = p->GetSize();
      // std::cout << "Seq: " << seq << ", PacketSize: " << pktSieInByte << std::endl;

      auto it = (*reorderDistTbl).find(fid);
      if (it == (*reorderDistTbl).end()) {
        reorderDistEntry * e = new reorderDistEntry(seq, pktSieInByte);
        (*reorderDistTbl)[fid] = *e;
      }else{
        it->second.counter += 1;
        it->second.size += pktSieInByte;
        it->second.lastTime = Now().GetNanoSeconds();
        if (seq >= it->second.maxValue) {
          it->second.maxIndex = it->second.counter;
          it->second.maxValue = seq;
        }else{
          uint32_t d = (it->second.maxValue-1-seq)/DEFAULT_MAX_TCP_MSS_IN_BYTE + 1;
          if (it->second.freq.size() < d) {
            it->second.freq.resize(d, 0);
          }
          it->second.freq[d-1] += 1;
        }
      }
}

TypeId
BulkSendApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BulkSendApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications") 
    .AddConstructor<BulkSendApplication> ()
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&BulkSendApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&BulkSendApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("Local",
                   "The Address on which to bind the socket. If not set, it is generated automatically.",
                   AddressValue (),
                   MakeAddressAccessor (&BulkSendApplication::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BulkSendApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&BulkSendApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("EnableSeqTsSizeHeader",
                   "Add SeqTsSizeHeader to each packet",
                   BooleanValue (false),
                   MakeBooleanAccessor (&BulkSendApplication::m_enableSeqTsSizeHeader),
                   MakeBooleanChecker ())
    .AddTraceSource ("Tx", "A new packet is sent",
                     MakeTraceSourceAccessor (&BulkSendApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("TxWithSeqTsSize", "A new packet is created with SeqTsSizeHeader",
                     MakeTraceSourceAccessor (&BulkSendApplication::m_txTraceWithSeqTsSize),
                     "ns3::PacketSink::SeqTsSizeCallback")
    .AddTraceSource ("AppStart", "A new application Starts",
                     MakeTraceSourceAccessor (&BulkSendApplication::m_traceWithAppStart),
                     "ns3::TracedCallback")
    .AddTraceSource ("AppComplete", "A new application Completes",
                     MakeTraceSourceAccessor (&BulkSendApplication::m_traceWithAppComplete),
                     "ns3::TracedCallback")
  ;
  return tid;
}

std::map<std::string, flowInfo> BulkSendApplication::flowTable;
std::map<std::string, reorderDistEntry> BulkSendApplication::reorderDistTbl;

void BulkSendApplication::PrintFlowTable(Ptr<OutputStreamWrapper> stream){
  
  std::vector<std::pair<std::string, flowInfo> > sortedVec(flowTable.begin(), flowTable.end());
  const uint32_t n_flow = sortedVec.size();
  uint64_t initialValue = 0;
  auto compareFlowInfoByIncreasingFct =  [](const std::pair<std::string, flowInfo>& a,  const std::pair<std::string, flowInfo>& b) {
          return (a.second.completeTimeInNs-a.second.startTimeInNs) < (b.second.completeTimeInNs-b.second.startTimeInNs); // 升序排序
  };
  std::sort(sortedVec.begin(), sortedVec.end(), compareFlowInfoByIncreasingFct);
  auto accumulateByFct = [](uint64_t acc, const std::pair<std::string, flowInfo>& s) {
    return acc + (s.second.completeTimeInNs-s.second.startTimeInNs);
  };
  uint64_t totalFctInNs = std::accumulate(sortedVec.begin(), sortedVec.end(), initialValue, accumulateByFct);
  double avgFctInNs = 1.0*totalFctInNs/n_flow;

  uint32_t smallFlowIndex = n_flow * SMALL_FLOW_RATIO;
  uint64_t smallFctInNs = std::accumulate(sortedVec.begin(), sortedVec.begin()+smallFlowIndex+1, initialValue, accumulateByFct);
  double avgSmallFctInNs = 1.0*smallFctInNs/(smallFlowIndex+1);

  uint32_t smallFlowIndex_99 = smallFlowIndex * 0.99;
  double smallFctInNs_99 =  sortedVec[smallFlowIndex_99].second.completeTimeInNs-sortedVec[smallFlowIndex_99].second.startTimeInNs;

  uint32_t largeFlowIndex = std::min(n_flow-1, uint32_t(n_flow * (1-LARGE_FLOW_RATIO)));
  uint64_t largeFctInNs = std::accumulate(sortedVec.begin()+largeFlowIndex, sortedVec.end(), initialValue, accumulateByFct);
  double avglargeFctInNs = 1.0*largeFctInNs/(n_flow-largeFlowIndex);

  uint32_t largeFlowIndex_99 = largeFlowIndex * 0.99;
  double largeFctInNs_99 =  sortedVec[largeFlowIndex_99].second.completeTimeInNs-sortedVec[largeFlowIndex_99].second.startTimeInNs;



  auto compareFlowInfoByDecreasingSize =  [](const std::pair<std::string, flowInfo>& a,  const std::pair<std::string, flowInfo>& b) {
          return a.second.recvBytes > b.second.recvBytes; // 降序排序
  };
  std::sort(sortedVec.begin(), sortedVec.end(), compareFlowInfoByDecreasingSize);
  auto accumulateBySize = [](uint64_t acc, const std::pair<std::string, flowInfo>& s) {
    return acc + s.second.recvBytes;
  };
  uint64_t bytes = std::accumulate(sortedVec.begin(), sortedVec.end(), initialValue, accumulateBySize);


  std::ostream* os = stream->GetStream ();
  *os << "******************************************************";
  *os << "Flow Tables of " << n_flow << " flows and " << bytes << "bytes";
  *os << "******************************************************";
  *os << "\n";

  *os << std::setiosflags (std::ios::left) << std::setw (12) << "n_total";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "Avg_FCT";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "n_Small";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "Small_FCT";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "n_Small_99";
  *os << std::setiosflags (std::ios::left) << std::setw (15) << "99_Small_FCT";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "n_large";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "Large_FCT";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "n_large_99";
  *os << std::setiosflags (std::ios::left) << std::setw (12) << "99_Large_FCT";
  *os << "\n";

  *os << std::setiosflags (std::ios::left) << std::setw (12) << n_flow;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << std::fixed << std::setprecision(1) << avgFctInNs;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << smallFlowIndex+1;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << std::fixed << std::setprecision(1) << avgSmallFctInNs;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << smallFlowIndex_99;
  *os << std::setiosflags (std::ios::left) << std::setw (15) << std::fixed << std::setprecision(1) << smallFctInNs_99;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << n_flow-largeFlowIndex;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << std::fixed << std::setprecision(1) << avglargeFctInNs;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << largeFlowIndex_99;
  *os << std::setiosflags (std::ios::left) << std::setw (12) << std::fixed << std::setprecision(1) << largeFctInNs_99;
  *os << "\n";

  *os << std::setiosflags (std::ios::left) << std::setw (10) << "Index";
  *os << std::setiosflags (std::ios::left) << std::setw (25) << "sNode-dNode-dPort";
  *os << std::setiosflags (std::ios::left) << std::setw (10) << "PDF_Byte";
  *os << std::setiosflags (std::ios::left) << std::setw (10) << "CDF_Flow";
  *os << std::setiosflags (std::ios::left) << std::setw (10) << "CDF_Byte";
  *os << std::setiosflags (std::ios::left) << std::setw (20) << "Byte_Send";
  *os << std::setiosflags (std::ios::left) << std::setw (20) << "Byte_Recv";
  *os << std::setiosflags (std::ios::left) << std::setw (20) << "T_start(Ns)";
  *os << std::setiosflags (std::ios::left) << std::setw (20) << "T_end(Ns)";
  *os << std::setiosflags (std::ios::left) << std::setw (20) << "T_last(Ns)";
  *os << "\n";


  uint64_t cumsum_bytes = 0;
  for (uint32_t i = 0; i < n_flow; i++) {
    cumsum_bytes += sortedVec[i].second.recvBytes;
    *os << std::setiosflags (std::ios::left) << std::setw (10) << i+1;
    *os << std::setiosflags (std::ios::left) << std::setw (25) << sortedVec[i].first;
    *os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << 1.0*sortedVec[i].second.recvBytes/bytes;
    *os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << 1.0*(i+1)/n_flow;
    *os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << 1.0*cumsum_bytes/bytes;
    *os << sortedVec[i].second.ToString();
    *os << "\n";
  }


  
}


BulkSendApplication::BulkSendApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0),
    m_unsentPacket (0)
{
  NS_LOG_FUNCTION (this);
}

BulkSendApplication::~BulkSendApplication ()
{
  NS_LOG_FUNCTION (this);
}

void BulkSendApplication::StartNotification(std::string flowId, uint64_t bytes){
  auto it = flowTable.find(flowId);
  if (it == flowTable.end()) {
    flowInfo * f = new flowInfo(flowId, bytes);
    flowTable[flowId] = *f;
    // if (skt != 0 && (DynamicCast<TcpSocketBase> (skt))!=0) {
    //   std::cout << "------------------------------------------------" << std::endl;
    //   (DynamicCast<TcpSocketBase> (skt))->TraceConnectWithoutContext ("Bx", MakeBoundCallback (&tcpSocketBaseBxCb, &reorderDistTbl, flowId));
    // }
  }else{
    std::cout << "Error in BulkSendApplication::StartNotification() with flow: " << flowId << ", size: " << bytes << std::endl;
  }
}

void BulkSendApplication::CompleteNotification(std::string flowId, uint64_t bytes){
  auto it = flowTable.find(flowId);
  if (it != flowTable.end()){
    flowTable[flowId].completeTimeInNs = Now().GetNanoSeconds();
    flowTable[flowId].recvBytes = bytes;
  }else{
    std::cout << "Error in ulkSendApplication::CompleteNotification() with flow: " << flowId << ", size: " << bytes << std::endl;
  }
}


void
BulkSendApplication::SetMaxBytes (uint64_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
BulkSendApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
BulkSendApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  m_unsentPacket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void BulkSendApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  Address from;

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      int ret = -1;

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                          "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (! m_local.IsInvalid())
        {
          NS_ABORT_MSG_IF ((Inet6SocketAddress::IsMatchingType (m_peer) && InetSocketAddress::IsMatchingType (m_local)) ||
                           (InetSocketAddress::IsMatchingType (m_peer) && Inet6SocketAddress::IsMatchingType (m_local)),
                           "Incompatible peer and local address IP version");
          ret = m_socket->Bind (m_local);
        }
      else
        {
          if (Inet6SocketAddress::IsMatchingType (m_peer))
            {
              ret = m_socket->Bind6 ();
            }
          else if (InetSocketAddress::IsMatchingType (m_peer))
            {
              ret = m_socket->Bind ();
            }
        }

      if (ret == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      m_traceWithAppStart(m_maxBytes);
      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&BulkSendApplication::ConnectionSucceeded, this),
        MakeCallback (&BulkSendApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&BulkSendApplication::DataSend, this));
    }
  if (m_connected)
    {
      m_socket->GetSockName (from);
      SendData (from, m_peer);
    }
}

void BulkSendApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("BulkSendApplication found null socket to close in StopApplication");
    }
}


// Private helpers

void BulkSendApplication::SendData (const Address &from, const Address &to)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more

      // uint64_t to allow the comparison later.
      // the result is in a uint32_t range anyway, because
      // m_sendSize is uint32_t.
      uint64_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (toSend, m_maxBytes - m_totBytes);
        }

      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());

      Ptr<Packet> packet;
      if (m_unsentPacket)
        {
          packet = m_unsentPacket;
          toSend = packet->GetSize ();
        }
      else if (m_enableSeqTsSizeHeader)
        {
          SeqTsSizeHeader header;
          header.SetSeq (m_seq++);
          header.SetSize (toSend);
          NS_ABORT_IF (toSend < header.GetSerializedSize ());
          packet = Create<Packet> (toSend - header.GetSerializedSize ());
          // Trace before adding header, for consistency with PacketSink
          m_txTraceWithSeqTsSize (packet, from, to, header);
          packet->AddHeader (header);
        }
      else
        {
          packet = Create<Packet> (toSend);
        }

      int actual = m_socket->Send (packet);
      if ((unsigned) actual == toSend)
        {
          m_totBytes += actual;
          m_txTrace (packet);
          m_unsentPacket = 0;
        }
      else if (actual == -1)
        {
          // We exit this loop when actual < toSend as the send side
          // buffer is full. The "DataSent" callback will pop when
          // some buffer space has freed up.
          NS_LOG_DEBUG ("Unable to send packet; caching for later attempt");
          m_unsentPacket = packet;
          break;
        }
      else if (actual > 0 && (unsigned) actual < toSend)
        {
          // A Linux socket (non-blocking, such as in DCE) may return
          // a quantity less than the packet size.  Split the packet
          // into two, trace the sent packet, save the unsent packet
          NS_LOG_DEBUG ("Packet size: " << packet->GetSize () << "; sent: " << actual << "; fragment saved: " << toSend - (unsigned) actual);
          Ptr<Packet> sent = packet->CreateFragment (0, actual);
          Ptr<Packet> unsent = packet->CreateFragment (actual, (toSend - (unsigned) actual));
          m_totBytes += actual;
          m_txTrace (sent);
          m_unsentPacket = unsent;
          break;
        }
      else
        {
          NS_FATAL_ERROR ("Unexpected return value from m_socket->Send ()");
        }
    }
  // Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && m_connected)
    {
      m_socket->Close ();
      m_connected = false;
      m_traceWithAppComplete(m_totBytes);
    }
}

void BulkSendApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendApplication Connection succeeded");
  m_connected = true;
  Address from, to;
  socket->GetSockName (from);
  socket->GetPeerName (to);
  SendData (from, to);
}

void BulkSendApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendApplication, Connection Failed");
}

void BulkSendApplication::DataSend (Ptr<Socket> socket, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      Address from, to;
      socket->GetSockName (from);
      socket->GetPeerName (to);
      SendData (from, to);
    }
}



} // Namespace ns3
