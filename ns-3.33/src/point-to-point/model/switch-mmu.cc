#include <iostream>
#include <fstream>
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/object-vector.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/global-value.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/random-variable-stream.h"
#include "switch-mmu.h"

NS_LOG_COMPONENT_DEFINE("SwitchMmu");
namespace ns3 {
	TypeId SwitchMmu::GetTypeId(void){
		static TypeId tid = TypeId("ns3::SwitchMmu")
			.SetParent<Object>()
			.AddConstructor<SwitchMmu>();
		return tid;
	}
	// bool SwitchMmu::isDynamicPfcThreshold = false;

	SwitchMmu::SwitchMmu(void){
		buffer_size = 12 * 1024 * 1024;
		reserve = 4 * 1024;
		resume_offset = 3 * 1024;

		// headroom
		shared_used_bytes = 0;
		memset(hdrm_bytes, 0, sizeof(hdrm_bytes));
		memset(ingress_bytes, 0, sizeof(ingress_bytes));
		memset(paused, 0, sizeof(paused));
		memset(egress_bytes, 0, sizeof(egress_bytes));
		m_SmartFlowRouting = CreateObject<RdmaSmartFlowRouting>();
		m_ConWeaveRouting = CreateObject<ConWeaveRouting>();

		if (!m_SmartFlowRouting)
		{
			NS_FATAL_ERROR("Failed to create RdmaSmartFlowRouting object.");
		}
		if (!m_ConWeaveRouting)
		{
			NS_FATAL_ERROR("Failed to create m_ConWeaveRouting object.");
		}
	}
	bool SwitchMmu::CheckIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		// if (psize + hdrm_bytes[port][qIndex] > headroom[port] && psize + GetSharedUsed(port, qIndex) > GetPfcThreshold(port)){
		// if (hdrm_bytes[port][qIndex] > 0)

		// {
		// 	if (ingress_bytes[port][qIndex] <= reserve || ingress_bytes[port][qIndex] <= reserve + shared_bytes)

		// 	{
		// 		std::cerr << "ingress_bytes[port][qIndex] <= reserve" << std::endl;
		// 		exit(1);
		// 	}
		// 	if (ingress_bytes[port][qIndex] != reserve + shared_bytes + hdrm_bytes[port][qIndex])

		// 	{
		// 		std::cerr << "ingress_bytes[port][qIndex] != reserve + shared_bytes + hdrm_bytes[port][qIndex]" << std::endl;
		// 		exit(1);
		// 	}
		// }

		if (psize + hdrm_bytes[port][qIndex] > headroom[port]){
			std::ostringstream oss;
			// oss << Simulator::Now().GetTimeStep() << " " << node_id << " Drop: queue:" << port << "," << qIndex << ": Headroom full";
			// NS_LOG_INFO(oss.str());

			// for (uint32_t i = 1; i < 64; i++)
			//{

			// std::cout << oss.str() << std::endl;
			// std::cout << " psize:" << psize << " reserve:" << reserve << " shared_bytes:" << shared_bytes << " headroom[port]:" << headroom[port];
			// std::cout << " hdrm_bytes[i][3]:" << hdrm_bytes[port][qIndex] << " ingress_bytes[i][3]:" << ingress_bytes[port][qIndex] << std::endl;

			// NS_LOG_INFO("(" << hdrm_bytes[i][3] << "," << ingress_bytes[i][3] << ")");
			//}
			return false;
		}
		return true;
	}
	bool SwitchMmu::CheckEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		return true;
	}
	void SwitchMmu::UpdateIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		uint32_t new_bytes = ingress_bytes[port][qIndex] + psize;
		if (new_bytes <= reserve){
			ingress_bytes[port][qIndex] += psize;
		}else {
			uint32_t thresh = GetPfcThreshold(port);
			if (new_bytes - reserve > thresh)
			{
				int32_t tmp_shared_inc = 0;
				if (ingress_bytes[port][qIndex] - reserve >= thresh)
				{
					tmp_shared_inc = 0;
				}
				else
				{
					tmp_shared_inc = thresh - (ingress_bytes[port][qIndex] - reserve);
				}
				if (tmp_shared_inc < 0)
				{
					std::cerr << "tmp_shared_inc < 0" << std::endl;
				}

				shared_used_bytes += tmp_shared_inc; // ying
				ingress_bytes[port][qIndex] += psize; // ying
				// hdrm_bytes[port][qIndex] += std::min(psize, new_bytes - reserve - thresh); //ying
				hdrm_bytes[port][qIndex] += psize - tmp_shared_inc; // ying
																	// hdrm_bytes[port][qIndex] += psize;
			}
			else
			{
				// uint32_t reserve_left = ingress_bytes[port][qIndex]
				ingress_bytes[port][qIndex] += psize;
				shared_used_bytes += std::min(psize, new_bytes - reserve);
			}
		}
	}
	void SwitchMmu::UpdateEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		egress_bytes[port][qIndex] += psize;
	}
	void SwitchMmu::RemoveFromIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		uint32_t from_hdrm = std::min(hdrm_bytes[port][qIndex], psize);
		uint32_t from_shared = std::min(psize - from_hdrm, ingress_bytes[port][qIndex] > reserve ? ingress_bytes[port][qIndex] - reserve : 0);
		hdrm_bytes[port][qIndex] -= from_hdrm;
		// ingress_bytes[port][qIndex] -= psize - from_hdrm;
		ingress_bytes[port][qIndex] -= psize; // ying
		shared_used_bytes -= from_shared;
	}
	void SwitchMmu::RemoveFromEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		egress_bytes[port][qIndex] -= psize;
	}
	bool SwitchMmu::CheckShouldPause(uint32_t port, uint32_t qIndex){
		return !paused[port][qIndex] && (hdrm_bytes[port][qIndex] > 0 || GetSharedUsed(port, qIndex) >= GetPfcThreshold(port));
	}
	bool SwitchMmu::CheckShouldResume(uint32_t port, uint32_t qIndex){
		if (!paused[port][qIndex])
			return false;
		uint32_t shared_used = GetSharedUsed(port, qIndex);
		return hdrm_bytes[port][qIndex] == 0 && (shared_used == 0 || shared_used + resume_offset <= GetPfcThreshold(port));
	}
	void SwitchMmu::SetPause(uint32_t port, uint32_t qIndex){
		paused[port][qIndex] = true;
	}
	void SwitchMmu::SetResume(uint32_t port, uint32_t qIndex){
		paused[port][qIndex] = false;
	}

	uint32_t SwitchMmu::GetPfcThreshold(uint32_t port){
		// return (buffer_size - total_hdrm - total_rsrv - shared_used_bytes) >> pfc_a_shift[port];
		return shared_bytes;

	}
	uint32_t SwitchMmu::GetSharedUsed(uint32_t port, uint32_t qIndex){
		uint32_t used = ingress_bytes[port][qIndex];
		if (used > reserve) // ying
		{
			return std::min(used - reserve, shared_bytes);
		}
		else	// ying
		{
			return 0; // ying
		} // ying
		// return used > reserve ? used - reserve : 0;
	}
	bool SwitchMmu::ShouldSendCN(uint32_t ifindex, uint32_t qIndex){
		if (qIndex == 0)
			return false;
		if (egress_bytes[ifindex][qIndex] > kmax[ifindex])
			return true;
		if (egress_bytes[ifindex][qIndex] > kmin[ifindex]){
			double p = pmax[ifindex] * double(egress_bytes[ifindex][qIndex] - kmin[ifindex]) / (kmax[ifindex] - kmin[ifindex]);
			Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
			if (x->GetValue(0, 1) < p)
				return true;
		}
		return false;
	}
	void SwitchMmu::ConfigEcn(uint32_t port, uint32_t _kmin, uint32_t _kmax, double _pmax){
		kmin[port] = _kmin * 1000;
		kmax[port] = _kmax * 1000;
		pmax[port] = _pmax;
	}
	void SwitchMmu::ConfigHdrm(uint32_t port, uint32_t size){
		headroom[port] = size;
	}
	void SwitchMmu::ConfigNPort(uint32_t n_port){
		total_hdrm = 0;
		total_rsrv = 0;
		for (uint32_t i = 1; i <= n_port; i++){
			total_hdrm += headroom[i];
			total_rsrv += reserve;
		}
		shared_bytes = (buffer_size - total_hdrm - total_rsrv)/n_port;// ying

	}
	void SwitchMmu::ConfigBufferSize(uint32_t size){
		buffer_size = size;
	}
}
