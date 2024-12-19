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

#ifndef BULK_SEND_APPLICATION_H
#define BULK_SEND_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/tcp-socket-base.h"

#include <iomanip>
// #include "ns3/userdefinedfunction.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdio>
#include <vector>
#include <map>
#include <utility>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
#include <functional>
#include <iostream>
#include <string>
#include <stdint.h>
#include <ctime>
#include <numeric>
#include <algorithm>

#define DEFAULT_MAX_TCP_MSS_IN_BYTE 1400
#define SMALL_FLOW_RATIO 0.5
#define LARGE_FLOW_RATIO 0.05

namespace ns3 {


template <typename T>
std::string local_vector_to_string (std::vector<T> src, std::string c=", ") {
  std::string str = "[";
  uint32_t vecSize = src.size();
  if (vecSize == 0) {
    str = str + "NULL]";
    return str;
  }else{
    for (uint32_t i = 0; i < vecSize-1; i++) {
      std::string curStr = std::to_string(src[i]);
      str = str + curStr + c;
    }
    std::string curStr = std::to_string(src[vecSize-1]);
    str = str + curStr + "]";
    return str;
  }
}




struct reorderDistEntry {
  std::vector<uint32_t> freq;
  uint64_t startTime;
  uint64_t lastTime;
  uint64_t maxValue;
  uint64_t maxIndex;
  uint32_t counter;
  uint64_t size;
  reorderDistEntry(){
    freq.clear();
    maxIndex = 0;
    maxValue = 0;
    counter = 0;
    size = 0;
  }
  bool operator==(const reorderDistEntry& other) const {
    return maxValue == other.maxValue && maxIndex == other.maxIndex && counter == other.counter&& size == other.size && startTime == other.startTime;
  }
  reorderDistEntry(uint32_t seq, uint32_t s){
    freq.clear();
    maxIndex = 1;
    maxValue = seq;
    counter = 1;
    size = s;
    startTime = Now().GetNanoSeconds();
    lastTime = Now().GetNanoSeconds();
  }
  void Print(std::ostream &os){
    os << std::setiosflags (std::ios::left) << std::setw (7) << counter;
    os << std::setiosflags (std::ios::left) << std::setw (10) << size;
    double avgPktSize = counter == 0 ? 0 : 1.0*size/counter;
    os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << avgPktSize;
    double avgPktGap = counter <= 1 ? 0 : 1.0*(lastTime-startTime)/(counter-1)/1000;
    os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << avgPktGap;
    double ratio = freq.size() == 0 ? 0 : 1.0*std::accumulate(freq.begin(), freq.end(), 0)/counter;
    os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << ratio;
    os << local_vector_to_string<uint32_t> (freq, ", ");
    os << std::endl;
  }

  std::string ToString(){
      std::ostringstream os;
      os << std::setiosflags (std::ios::left) << std::setw (7) << counter;
      os << std::setiosflags (std::ios::left) << std::setw (10) << size;
      double avgPktSize = counter == 0 ? 0 : 1.0*size/counter;
      os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << avgPktSize;
      double avgPktGap = counter <= 1 ? 0 : 1.0*(lastTime-startTime)/(counter-1)/1000;
      os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << avgPktGap;
      double ratio = freq.size() == 0 ? 0 : 1.0*std::accumulate(freq.begin(), freq.end(), 0)/counter;
      os << std::setiosflags (std::ios::left) << std::setw (10) << std::fixed << std::setprecision(4) << ratio;
      os << local_vector_to_string<uint32_t> (freq, ", ");
      return os.str();
  }


};

// void tcpSocketBaseBxCb (std::map<std::string, reorderDistEntry> * reorderDistTbl, std::string fid, Ptr<const Packet> p, const TcpHeader& tcpHdr,  Ptr<const TcpSocketBase> skt);
void tcpSocketBaseBxCb (std::string fid, std::map<std::string, reorderDistEntry> * reorderDistTbl, Ptr<const Packet> p, const TcpHeader& tcpHdr);


struct flowInfo {
  std::string flowId;
  uint64_t sendBytes;
  uint64_t recvBytes;
  uint64_t startTimeInNs;
  uint64_t completeTimeInNs;
  flowInfo(){
    flowId = "";
    sendBytes = 0;
    recvBytes = 0;
    startTimeInNs = 0;
    completeTimeInNs = 0;
  }

  flowInfo(std::string fid, uint64_t bytes){
    flowId = fid;
    sendBytes = bytes;
    recvBytes = 0;
    startTimeInNs = Now().GetNanoSeconds();
    completeTimeInNs = 0;
  }

  void Print(Ptr<OutputStreamWrapper> stream){
    std::ostream* os = stream->GetStream ();
    *os << std::setiosflags (std::ios::left) << std::setw (20) << flowId;
    *os << std::setiosflags (std::ios::left) << std::setw (20) << sendBytes;
    *os << std::setiosflags (std::ios::left) << std::setw (20) << recvBytes;
    *os << std::setiosflags (std::ios::left) << std::setw (20) << startTimeInNs;
    *os << std::setiosflags (std::ios::left) << std::setw (20) << completeTimeInNs;
    *os << std::setiosflags (std::ios::left) << std::setw (20) << completeTimeInNs-startTimeInNs;

    *os << "\n";
  }
  
  std::string ToString(){
      std::ostringstream os;
      os << std::setiosflags (std::ios::left) << std::setw (20) << sendBytes;
      os << std::setiosflags (std::ios::left) << std::setw (20) << recvBytes;
      os << std::setiosflags (std::ios::left) << std::setw (20) << startTimeInNs;
      os << std::setiosflags (std::ios::left) << std::setw (20) << completeTimeInNs;
      os << std::setiosflags (std::ios::left) << std::setw (20) << completeTimeInNs-startTimeInNs;
      return os.str();
  }
};

class Address;
class Socket;

/**
 * \ingroup applications
 * \defgroup bulksend BulkSendApplication
 *
 * This traffic generator simply sends data
 * as fast as possible up to MaxBytes or until
 * the application is stopped (if MaxBytes is
 * zero). Once the lower layer send buffer is
 * filled, it waits until space is free to
 * send more data, essentially keeping a
 * constant flow of data. Only SOCK_STREAM 
 * and SOCK_SEQPACKET sockets are supported. 
 * For example, TCP sockets can be used, but 
 * UDP sockets can not be used.
 */

/**
 * \ingroup bulksend
 *
 * \brief Send as much traffic as possible, trying to fill the bandwidth.
 *
 * This traffic generator simply sends data
 * as fast as possible up to MaxBytes or until
 * the application is stopped (if MaxBytes is
 * zero). Once the lower layer send buffer is
 * filled, it waits until space is free to
 * send more data, essentially keeping a
 * constant flow of data. Only SOCK_STREAM
 * and SOCK_SEQPACKET sockets are supported.
 * For example, TCP sockets can be used, but
 * UDP sockets can not be used.
 *
 * If the attribute "EnableSeqTsSizeHeader" is enabled, the application will
 * use some bytes of the payload to store an header with a sequence number,
 * a timestamp, and the size of the packet sent. Support for extracting 
 * statistics from this header have been added to \c ns3::PacketSink 
 * (enable its "EnableSeqTsSizeHeader" attribute), or users may extract
 * the header via trace sources.
 */
class BulkSendApplication : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static std::map<std::string, flowInfo> flowTable;
  static std::map<std::string, reorderDistEntry> reorderDistTbl;

  static void PrintFlowTable(Ptr<OutputStreamWrapper> stream);
  BulkSendApplication ();

  virtual ~BulkSendApplication ();

  // typedef void (* AppStartCallback) (const Ptr<const Socket>, const uint64_t );

  /**
   * \brief Set the upper bound for the total number of bytes to send.
   *
   * Once this bound is reached, no more application bytes are sent. If the
   * application is stopped during the simulation and restarted, the 
   * total number of bytes sent is not reset; however, the maxBytes 
   * bound is still effective and the application will continue sending 
   * up to maxBytes. The value zero for maxBytes means that 
   * there is no upper bound; i.e. data is sent until the application 
   * or simulation is stopped.
   *
   * \param maxBytes the upper bound of bytes to send
   */
  void SetMaxBytes (uint64_t maxBytes);

  /**
   * \brief Get the socket this application is attached to.
   * \return pointer to associated socket
   */
  Ptr<Socket> GetSocket (void) const;
  void StartNotification(std::string context, uint64_t bytes);
  void CompleteNotification(std::string context, uint64_t bytes);

protected:
  virtual void DoDispose (void);
private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  /**
   * \brief Send data until the L4 transmission buffer is full.
   * \param from From address
   * \param to To address
   */
  void SendData (const Address &from, const Address &to);

  Ptr<Socket>     m_socket;       //!< Associated socket
  Address         m_peer;         //!< Peer address
  Address         m_local;        //!< Local address to bind to
  bool            m_connected;    //!< True if connected
  uint32_t        m_sendSize;     //!< Size of data to send each time
  uint64_t        m_maxBytes;     //!< Limit total number of bytes sent
  uint64_t        m_totBytes;     //!< Total bytes sent so far
  TypeId          m_tid;          //!< The type of protocol to use.
  uint32_t        m_seq {0};      //!< Sequence
  Ptr<Packet>     m_unsentPacket; //!< Variable to cache unsent packet
  bool            m_enableSeqTsSizeHeader {false}; //!< Enable or disable the SeqTsSizeHeader

  /// Traced Callback: sent packets
  TracedCallback<Ptr<const Packet> > m_txTrace;
  TracedCallback<Ptr<const Packet>, const Address &, const Address &, const SeqTsSizeHeader &> m_txTraceWithSeqTsSize;

  /// Callback for tracing the packet Tx events, includes source, destination,  the packet sent, and header
  TracedCallback<uint64_t > m_traceWithAppStart;
  TracedCallback<uint64_t > m_traceWithAppComplete;

private:
  /**
   * \brief Connection Succeeded (called by Socket through a callback)
   * \param socket the connected socket
   */
  void ConnectionSucceeded (Ptr<Socket> socket);
  /**
   * \brief Connection Failed (called by Socket through a callback)
   * \param socket the connected socket
   */
  void ConnectionFailed (Ptr<Socket> socket);
  /**
   * \brief Send more data as soon as some has been transmitted.
   */
  void DataSend (Ptr<Socket>, uint32_t); // for socket's SetSendCallback
};

} // namespace ns3

#endif /* BULK_SEND_APPLICATION_H */
