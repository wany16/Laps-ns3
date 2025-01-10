#include "common-user-model.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("CommonUserModel");

    std::string GetStringHashValueFromCustomHeader(const CustomHeader &ch)
    {
        uint16_t port = 0;
        if (ch.l3Prot == 0x6) // TCP
            port = ch.tcp.sport;
        else if (ch.l3Prot == 0x11) // UDP
            port = ch.udp.sport;
        else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) // ACK or NACK
            port = ch.ack.sport;
        else
        {
            std::cout << "Error in GetStringHashValueFromCustomHeader() for UNKOWN header type" << std::endl;
            return "";
        }
        std::string stringhash = std::to_string(ch.sip) + "#" + std::to_string(ch.dip) + "#" + std::to_string(port); // srcPort=dstPort
        return stringhash;
    }

    RoutePath::RoutePath()
    {
        m_pathSelTbl.clear();
        m_nexthopSelTbl.clear();
        m_vmVtepMapTbl.clear();
    }
    RoutePath::~RoutePath() {}
    uint32_t RoutePath::install_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &vmt)
    {
        set_SMT(vmt);
        return vmt.size();
    }
    void RoutePath::set_SMT(std::map<Ipv4Address, hostIp2SMT_entry_t> &vmVtepMapTbl)
    {
        m_vmVtepMapTbl.insert(vmVtepMapTbl.begin(), vmVtepMapTbl.end());
        // NS_LOG_INFO ("m_vmVtepMapTbl size " << m_vmVtepMapTbl.size());
        return;
    }
   
    hostIp2SMT_entry_t *RoutePath::lookup_SMT(const Ipv4Address &serverAddr)
    {
        // NS_LOG_INFO ("############ Fucntion: lookup_VMT() ############");
        // NS_LOG_INFO("VMT_KEY: svAddr=" << serverAddr);
        // hostIp2SMT_entry_t vmtEntry;

        std::map<Ipv4Address, hostIp2SMT_entry_t>::iterator it;
        it = m_vmVtepMapTbl.find(serverAddr);
        if (it == m_vmVtepMapTbl.end())
        {
            std::cout << "Error in lookup_SMT() since Cannot match any entry in SMT for the Key: (";
            std::cout << ipv4Address2string(serverAddr);
            std::cout << std::endl;
            return 0;
        }
        else
        {
            return &(it->second);
        }
    }
    std::string ipv4Address2string(Ipv4Address addr)
    {
        std::ostringstream os;
        addr.Print(os);
        std::string addrStr = os.str();
        return addrStr;
    }
    std::string construct_target_string_strlen(uint32_t strLen, std::string c)
    {
        std::string result;
        for (uint32_t i = 0; i < strLen; ++i)
        {
            result = result + c;
        }
        return result;
    }

    uint32_t RoutePath::print_PIT()
    {
        uint32_t pitSize = m_nexthopSelTbl.size();
        uint32_t nodeID = m_switch_id;
        std::cout << "Node " << nodeID << " has a PIT of " << pitSize << " entries" << std::endl;
        std::cout << "Index" << construct_target_string_strlen(2, " ");
        std::cout << "Pid" << construct_target_string_strlen(3, " ");
        std::cout << "Priority" << construct_target_string_strlen(2, " ");
        std::cout << "Latency" << construct_target_string_strlen(3, " ");
        std::cout << "GenTime" << construct_target_string_strlen(3, " ");
        std::cout << "PbnTime" << construct_target_string_strlen(3, " ");
        std::cout << "SntTime" << construct_target_string_strlen(3, " ");
        std::cout << "NodeID" << construct_target_string_strlen(20, " ");
        std::cout << "PortID" << construct_target_string_strlen(1, " ");
        std::cout << std::endl;
        uint32_t pathIdx = 0;
        std::map<uint32_t, PathData>::iterator it;
        for (it = m_nexthopSelTbl.begin(); it != m_nexthopSelTbl.end(); it++)
        {
            std::cout << pathIdx << construct_target_string_strlen(5 + 2 - change2string(pathIdx).size(), " ");
            PathData *pstEntry = &(it->second);
            uint32_t pid = pstEntry->pid;
            std::cout << pid << construct_target_string_strlen(3 + 3 - change2string(pid).size(), " ");
            uint32_t priority = pstEntry->priority;
            std::cout << priority << construct_target_string_strlen(8 + 2 - change2string(priority).size(), " ");
            uint32_t latency = pstEntry->latency;
            std::cout << latency << construct_target_string_strlen(7 + 3 - change2string(latency).size(), " ");
            uint32_t GenTime = pstEntry->tsGeneration.GetNanoSeconds();
            std::cout << GenTime << construct_target_string_strlen(7 + 3 - change2string(GenTime).size(), " ");
            uint32_t PbnTime = pstEntry->tsProbeLastSend.GetNanoSeconds();
            std::cout << PbnTime << construct_target_string_strlen(7 + 3 - change2string(PbnTime).size(), " ");
            uint32_t SntTime = pstEntry->tsLatencyLastSend.GetNanoSeconds();
            std::cout << SntTime << construct_target_string_strlen(7 + 3 - change2string(SntTime).size(), " ");
            std::string nodeIdsStr = vectorTostring<uint32_t>(pstEntry->nodeIdSequence);
            std::cout << nodeIdsStr << construct_target_string_strlen(20 + 6 - change2string(nodeIdsStr).size(), " ");
            std::string portsStr = vectorTostring<uint32_t>(pstEntry->portSequence);
            std::cout << portsStr << construct_target_string_strlen(1, " ");
            std::cout << std::endl;
            pathIdx = pathIdx + 1;
        }
        return pathIdx;
    }
    
    uint32_t RoutePath::print_SMT()
    {
        uint32_t nodeID = m_switch_id;
        uint32_t vmtSize = m_vmVtepMapTbl.size();
        std::cout << "Node: **" << nodeID << "** has SMT in smartFlow with **" << vmtSize << "** entries" << std::endl;
        std::cout << "Index" << construct_target_string_strlen(5, " ");
        std::cout << "Server Address" << construct_target_string_strlen(6, " ");
        std::cout << "ToR Address" << construct_target_string_strlen(1, " ");
        std::cout << std::endl;
        uint32_t entryCnt = 0;
        std::map<Ipv4Address, hostIp2SMT_entry_t>::const_iterator it;
        for (it = m_vmVtepMapTbl.begin(); it != m_vmVtepMapTbl.end(); it++)
        {
            std::cout << entryCnt << construct_target_string_strlen(5 + 5 - change2string(entryCnt).size(), " ");
            std::cout << ipv4Address2string(it->first) << construct_target_string_strlen(6 + 14 - ipv4Address2string(it->first).size(), " ");
            std::cout << it->second.torId << construct_target_string_strlen(1, " ");
            std::cout << it->second.hostId << construct_target_string_strlen(1, " ");
            std::cout << std::endl;
            entryCnt = entryCnt + 1;
        }
        return entryCnt;
    }

    uint32_t RoutePath::install_PST(std::map<HostId2PathSeleKey, pstEntryData> &pst)
    {
        set_PST(pst);
        return pst.size();
    }

    void RoutePath::set_PST(std::map<HostId2PathSeleKey, pstEntryData> &pathSelTbl)
    {
        m_pathSelTbl.insert(pathSelTbl.begin(), pathSelTbl.end());
        // NS_LOG_INFO ("m_pathSelTbl size " << m_pathSelTbl.size());
        return;
    }

    pstEntryData *RoutePath::lookup_PST(HostId2PathSeleKey &pstKey)
    {
        // NS_LOG_INFO ("############ Fucntion: lookup_PST() ############");
        std::map<HostId2PathSeleKey, pstEntryData>::iterator it;
        it = m_pathSelTbl.find(pstKey);
        if (it == m_pathSelTbl.end())
        {
            std::cout << "Error in lookup_PST() since Cannot match any entry in PST for the Key: (";
            std::cout << pstKey.selfHostId;
            std::cout << ", " << pstKey.dstHostId << ")" << std::endl;
            return 0;
        }
        else
        {
            return &(it->second);
        }
    }
    uint32_t RoutePath::install_PIT(std::map<uint32_t, PathData> &pit)
    {
        set_PIT(pit);
        return pit.size();
    }
    void RoutePath::set_PIT(std::map<uint32_t, PathData> &nexthopSelTbl)
    {
        m_nexthopSelTbl.insert(nexthopSelTbl.begin(), nexthopSelTbl.end());
        // NS_LOG_INFO ("m_nexthopSelTbl size " << m_nexthopSelTbl.size());
        return;
    }
    PathData *RoutePath::lookup_PIT(uint32_t pieKey)
    {
        // NS_LOG_INFO ("############ Fucntion: lookup_PIT() ############");
        std::map<uint32_t, PathData>::iterator it;
        it = m_nexthopSelTbl.find(pieKey);
        if (it == m_nexthopSelTbl.end())
        {
            std::cout << "Error in lookup_PIT() since Cannot match any entry in PIT for the Key: ";
            std::cout << pieKey;
            std::cout << std::endl;
            return 0;
        }
        else
        {
            return &(it->second);
        }
    }

}