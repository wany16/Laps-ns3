#ifndef TRACE_FORMAT_H
#define TRACE_FORMAT_H
#include <stdint.h>
#include <cstdio>
#include <cassert>
#include <vector>
#include <stddef.h>
#include <cstring>

namespace ns3 {

enum TraceEvent {
	Recv = 0,
	Enqu = 1,
	Dequ = 2,
	Drop = 3
};

static inline const char* eventToStr(uint8_t t){
switch (t){
	case 0:
		return "Recv";
	case 1:
		return "Enqu";
	case 2:
		return "Dequ";
	case 3:
		return "Drop";
	default:
		return "????";
}
}

struct TraceFormat {
	uint64_t time;
	uint16_t node;
	uint8_t intf, qidx;
	uint32_t qlen;
	uint32_t sip, dip;
	uint16_t size;
	uint8_t l3Prot;
	uint8_t event;
	uint8_t ecn; // this is the ip ECN bits
	uint8_t nodeType; // 0: host, 1: switch
	union{
		struct {
			uint16_t sport, dport;
			uint32_t seq;
			uint64_t ts;
			uint16_t pg;
			uint16_t payload; // this does not include SeqTsHeader's size, diff from udp's payload size.
		} data;
		struct {
			uint16_t fid;
			uint8_t qIndex;
			uint8_t ecnBits; // this is the ECN bits in the CNP
			union{
				struct {
					uint16_t qfb;
					uint16_t total;
				};
				uint32_t seq;
			};
		} cnp;
		struct {
			uint16_t sport, dport;
			uint16_t flags;
			uint16_t pg;
			uint32_t seq;
			uint64_t ts;
		  uint32_t irnNack;
		  uint16_t irnNackSize;
		} ack;
		struct {
			uint32_t time;
			uint32_t qlen;
			uint8_t qIndex;
		} pfc;
		struct {
			uint16_t sport, dport;
		}qp;
	};

 const char* NodeTypeToStr(uint8_t t){
	switch (t){
		case 0:
			return "Host";
		case 1:
			return "Switch";
		default:
			return "????";
	}
}

 const char* PktTypeToStr(uint8_t t){
	switch (t){
		case 0x06:
			return "TCP";
		case 0x11:
			return "UDP";
		case 0xFD:
			return "NACK";
		case 0xFC:
			return "ACK";
		case 0xFF:
			return "CNP";
		case 0xFE:
			return "PFC";
		default:
			return "????";
	}
}

//  const char* Ipv4ToStr(uint32_t ip){
// 		uint8_t byte1 = (ip >> 24) & 0xFF;  // 高8位
//     uint8_t byte2 = (ip >> 16) & 0xFF;  // 次高8位
//     uint8_t byte3 = (ip >> 8) & 0xFF;   // 次低8位
//     uint8_t byte4 = ip & 0xFF;          // 低8位

//     // 构建字符串
//     snprintf(buf, 16, "%u.%u.%u.%u", byte1, byte2, byte3, byte4);

//     return buf;
// }


	void Serialize(FILE *file){
		// fwrite(this, sizeof(TraceFormat), 1, file);




        // fprintf(file, "Node:%d NodeType:%d Complete:%d srcNode:%d dstNode:%d DataSizeInByte:%lu PktSizeInByte:%u StartTimeInNs:%lu LastTimeInNs:%lu EndTimeInNs:%lu\n",
        //         node, kvApp->roundCnt, kvApp->completeCnt, srcNode->GetId(), dstNode->GetId(), q->m_size, totalBytes, q->startTime.GetNanoSeconds(),
        //         (Simulator::Now() - q->startTime).GetNanoSeconds(), Simulator::Now().GetNanoSeconds());

        fprintf(file, "Node:%d ", node);
        fprintf(file, "NodeType:%s ", NodeTypeToStr(nodeType));
        fprintf(file, "Nic:%d ", intf);		
        fprintf(file, "Qidx:%d ", qidx);
        fprintf(file, "Time:%lu ", time);
        fprintf(file, "Event:%s ", eventToStr(event));

				fprintf(file, "Sip:%d.%d.%d.%d ", (sip>>24)&0xFF, (sip>>16)&0xFF, (sip>>8)&0xFF, sip&0xFF); 
				fprintf(file, "Dip:%d.%d.%d.%d ", (dip>>24)&0xFF, (dip>>16)&0xFF, (dip>>8)&0xFF, dip&0xFF); 
				// fprintf(file, "PktType:%s ", PktTypeToStr(l3Prot));
				switch (l3Prot){
					case 0x06:
        		fprintf(file, "PktType:TCP ");
						fprintf(file, "SrcPort:%d ", data.sport);
						fprintf(file, "DstPort:%d ", data.dport);
						break;
					case 0x11:
        		fprintf(file, "PktType:Data ");
						fprintf(file, "SrcPort:%d ", data.sport);
						fprintf(file, "DstPort:%d ", data.dport);
						fprintf(file, "PG:%d ", data.pg);
						fprintf(file, "Seq:%d ", data.seq);
						break;
					case 0xFD:
        		fprintf(file, "PktType:NACK ");
						fprintf(file, "SrcPort:%d ", ack.sport);
						fprintf(file, "DstPort:%d ", ack.dport);
						fprintf(file, "PG:%d ", ack.pg);
						fprintf(file, "Seq:%d ", ack.seq);
						fprintf(file, "Ts:%lu ", ack.ts);
						fprintf(file, "IrnSeq:%d ", ack.irnNack);
						fprintf(file, "IrnLen:%d ", ack.irnNackSize);
						break;
					case 0xFC:
        		fprintf(file, "PktType:ACK ");
						fprintf(file, "SrcPort:%d ", ack.sport);
						fprintf(file, "DstPort:%d ", ack.dport);
						fprintf(file, "PG:%d ", ack.pg);
						fprintf(file, "Seq:%d ", ack.seq);
						fprintf(file, "Ts:%lu ", ack.ts);
						break;
					case 0xFF:
        		fprintf(file, "PktType:CNP ");
						fprintf(file, "Fid:%d ",cnp.fid);
						fprintf(file, "Qidx:%d ", cnp.qIndex);
						fprintf(file, "QFB:%d ", cnp.qfb);
						fprintf(file, "ECN:%d ", cnp.ecnBits);
						fprintf(file, "Total:%d ", cnp.total);
						break;
					case 0xFE:
        		fprintf(file, "PktType:PFC ");
						fprintf(file, "Time:%d ",pfc.time);
						fprintf(file, "Qlen:%d ", pfc.qlen);
						fprintf(file, "	Qidx:%d ", pfc.qIndex);
						fprintf(file, "ECN:%d ", cnp.ecnBits);
						fprintf(file, "Total:%d ", cnp.total);
						break;
					default:
						fprintf(file, "???? ");
				}
        fprintf(file, "Size:%d ", size);
        fprintf(file, "Qlen:%d\n", qlen);
        fflush(file);

	}
	int Deserialize(FILE *file){
		int ret = fread(this, sizeof(TraceFormat), 1, file);
		return ret;
	}
};

static inline const char* EventToStr(enum TraceEvent e){
	switch (e){
		case Recv:
			return "Recv";
		case Enqu:
			return "Enqu";
		case Dequ:
			return "Dequ";
		case Drop:
			return "Drop";
		default:
			return "????";
	}
}

}
#endif
