#include "rdma-hw.h"

#include "pint.h"
#include "ppp-header.h"
#include "qbb-header.h"

#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/feedback-tag.h"
#include "ns3/pointer.h"
#include "ns3/ppp-header.h"
#include "ns3/uinteger.h"
#include "ns3/unsched-tag.h"
#include <ns3/ipv4-header.h>
#include <ns3/seq-ts-header.h>
#include <ns3/simulator.h>
#include <ns3/udp-header.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <random>

namespace ns3
{

TypeId
RdmaHw::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::RdmaHw")
            .SetParent<Object>()
            .AddAttribute("MinRate",
                          "Minimum rate of a throttled flow",
                          DataRateValue(DataRate("100Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_minRate),
                          MakeDataRateChecker())
            .AddAttribute("Mtu",
                          "Mtu.",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&RdmaHw::m_mtu),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("CcMode",
                          "which mode of DCQCN is running",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RdmaHw::m_cc_mode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("NACKGenerationInterval",
                          "The NACK Generation interval",
                          DoubleValue(500.0),
                          MakeDoubleAccessor(&RdmaHw::m_nack_interval),
                          MakeDoubleChecker<double>())
            .AddAttribute("L2ChunkSize",
                          "Layer 2 chunk size. Disable chunk mode if equals to 0.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RdmaHw::m_chunk),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("L2AckInterval",
                          "Layer 2 Ack intervals. Disable ack if equals to 0.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RdmaHw::m_ack_interval),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("L2BackToZero",
                          "Layer 2 go back to zero transmission.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_backto0),
                          MakeBooleanChecker())
            .AddAttribute("EwmaGain",
                          "Control gain parameter which determines the level of rate decrease",
                          DoubleValue(1.0 / 16),
                          MakeDoubleAccessor(&RdmaHw::m_g),
                          MakeDoubleChecker<double>())
            .AddAttribute("RateOnFirstCnp",
                          "the fraction of rate on first CNP",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&RdmaHw::m_rateOnFirstCNP),
                          MakeDoubleChecker<double>())
            .AddAttribute("ClampTargetRate",
                          "Clamp target rate.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_EcnClampTgtRate),
                          MakeBooleanChecker())
            .AddAttribute("RPTimer",
                          "The rate increase timer at RP in microseconds",
                          DoubleValue(1500.0),
                          MakeDoubleAccessor(&RdmaHw::m_rpgTimeReset),
                          MakeDoubleChecker<double>())
            .AddAttribute("RateDecreaseInterval",
                          "The interval of rate decrease check",
                          DoubleValue(4.0),
                          MakeDoubleAccessor(&RdmaHw::m_rateDecreaseInterval),
                          MakeDoubleChecker<double>())
            .AddAttribute("FastRecoveryTimes",
                          "The rate increase timer at RP",
                          UintegerValue(5),
                          MakeUintegerAccessor(&RdmaHw::m_rpgThreshold),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("AlphaResumInterval",
                          "The interval of resuming alpha",
                          DoubleValue(55.0),
                          MakeDoubleAccessor(&RdmaHw::m_alpha_resume_interval),
                          MakeDoubleChecker<double>())
            .AddAttribute("RateAI",
                          "Rate increment unit in AI period",
                          DataRateValue(DataRate("5Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_rai),
                          MakeDataRateChecker())
            .AddAttribute("RateHAI",
                          "Rate increment unit in hyperactive AI period",
                          DataRateValue(DataRate("50Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_rhai),
                          MakeDataRateChecker())
            .AddAttribute("VarWin",
                          "Use variable window size or not",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_var_win),
                          MakeBooleanChecker())
            .AddAttribute("FastReact",
                          "Fast React to congestion feedback",
                          BooleanValue(true),
                          MakeBooleanAccessor(&RdmaHw::m_fast_react),
                          MakeBooleanChecker())
            .AddAttribute("MiThresh",
                          "Threshold of number of consecutive AI before MI",
                          UintegerValue(5),
                          MakeUintegerAccessor(&RdmaHw::m_miThresh),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("TargetUtil",
                          "The Target Utilization of the bottleneck bandwidth, by default 95%",
                          DoubleValue(0.95),
                          MakeDoubleAccessor(&RdmaHw::m_targetUtil),
                          MakeDoubleChecker<double>())
            .AddAttribute(
                "UtilHigh",
                "The upper bound of Target Utilization of the bottleneck bandwidth, by default 98%",
                DoubleValue(0.98),
                MakeDoubleAccessor(&RdmaHw::m_utilHigh),
                MakeDoubleChecker<double>())
            .AddAttribute("RateBound",
                          "Bound packet sending by rate, for test only",
                          BooleanValue(true),
                          MakeBooleanAccessor(&RdmaHw::m_rateBound),
                          MakeBooleanChecker())
            .AddAttribute("MultiRate",
                          "Maintain multiple rates in HPCC",
                          BooleanValue(true),
                          MakeBooleanAccessor(&RdmaHw::m_multipleRate),
                          MakeBooleanChecker())
            .AddAttribute("SampleFeedback",
                          "Whether sample feedback or not",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_sampleFeedback),
                          MakeBooleanChecker())
            .AddAttribute("TimelyAlpha",
                          "Alpha of TIMELY",
                          DoubleValue(0.875),
                          MakeDoubleAccessor(&RdmaHw::m_tmly_alpha),
                          MakeDoubleChecker<double>())
            .AddAttribute("TimelyBeta",
                          "Beta of TIMELY",
                          DoubleValue(0.8),
                          MakeDoubleAccessor(&RdmaHw::m_tmly_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("TimelyTLow",
                          "TLow of TIMELY (ns)",
                          UintegerValue(50000),
                          MakeUintegerAccessor(&RdmaHw::m_tmly_TLow),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("TimelyTHigh",
                          "THigh of TIMELY (ns)",
                          UintegerValue(500000),
                          MakeUintegerAccessor(&RdmaHw::m_tmly_THigh),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("TimelyMinRtt",
                          "MinRtt of TIMELY (ns)",
                          UintegerValue(20000),
                          MakeUintegerAccessor(&RdmaHw::m_tmly_minRtt),
                          MakeUintegerChecker<uint64_t>())
            // 50us RTTref comes from paper SQCC: Stable Queue Congestion Control
            .AddAttribute("TimelyPatchedRttRef", //
                          "RTTref of Patched TIMELY (ns)",
                          UintegerValue(500000),
                          MakeUintegerAccessor(&RdmaHw::m_ptmly_RttRef),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("TimelyPatchedBeta",
                          "Beta of PatchedTIMELY",
                          DoubleValue(0.008),
                          MakeDoubleAccessor(&RdmaHw::m_ptmly_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("DctcpRateAI",
                          "DCTCP's Rate increment unit in AI period",
                          DataRateValue(DataRate("1000Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_dctcp_rai),
                          MakeDataRateChecker())
            .AddAttribute("PintSmplThresh",
                          "PINT's sampling threshold in rand()%65536",
                          UintegerValue(65536),
                          MakeUintegerAccessor(&RdmaHw::pint_smpl_thresh),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("PowerTCPEnabled",
                          "to enable PowerTCP",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::PowerTCPEnabled),
                          MakeBooleanChecker())
            .AddAttribute("PowerTCPdelay",
                          "to enable PowerTCP in delaymode",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::PowerTCPdelay),
                          MakeBooleanChecker())
            // Swift parameters from code of paper Burst-tolerant datacenter networks with Vertigo
            .AddAttribute("SwiftAi",
                          "Swift's additive increment",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&RdmaHw::swift_ai),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("SwiftBeta",
                          "Swift's multiplicative decrease constant",
                          DoubleValue(0.8),
                          MakeDoubleAccessor(&RdmaHw::swift_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftMaxMdf",
                          "Swift's maximum multiplicative decrease factor",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&RdmaHw::swift_max_mdf),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftBaseTarget",
                          "Swift's base fabric target RTT (ns)",
                          UintegerValue(60000),
                          MakeUintegerAccessor(&RdmaHw::swift_base_target),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("SwiftHopScale",
                          "Swift's per hop RTT scaling factor",
                          DoubleValue(30000),
                          MakeDoubleAccessor(&RdmaHw::swift_hop_scale),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftFsMaxCwnd",
                          "Swift's max cwnd limit that enables flow-based scaling",
                          DoubleValue(100.0),
                          MakeDoubleAccessor(&RdmaHw::swift_fs_max_cwnd),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftFsMinCwnd",
                          "Swift's min cwnd limit that enables flow-based scaling",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&RdmaHw::swift_fs_min_cwnd),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftFsRange",
                          "Swift's flow-based scheduling max scaling range",
                          DoubleValue(0.000300),
                          MakeDoubleAccessor(&RdmaHw::swift_fs_range),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftMinCwnd",
                          "Swift's min cwnd it can exceed (not fs)",
                          DoubleValue(0.001),
                          MakeDoubleAccessor(&RdmaHw::swift_min_cwnd),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftMaxCwnd",
                          "Swift's max cwnd it can exceed (not fs)",
                          DoubleValue(800000),
                          MakeDoubleAccessor(&RdmaHw::swift_max_cwnd),
                          MakeDoubleChecker<double>())
            // seems that in emulator there is no endpoint delay
            .AddAttribute("SwiftTargetEndpointDelay",
                          "Swift's target endpoint delay (ns)",
                          UintegerValue(1000000),
                          MakeUintegerAccessor(&RdmaHw::swift_target_endpoint_delay),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("RttQcnTmin",
                          "RTT-QCN's max RTT value to generate no ECN",
                          UintegerValue(3000),
                          MakeUintegerAccessor(&RdmaHw::rtt_qcn_tmin),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RttQcnTmax",
                          "RTT-QCN's min RTT value to generate full ECN",
                          UintegerValue(5000),
                          MakeUintegerAccessor(&RdmaHw::rtt_qcn_tmax),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RttQcnAlpha",
                          "Additive increase when cwnd < mss",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&RdmaHw::rtt_qcn_alpha),
                          MakeDoubleChecker<double>())
            .AddAttribute("RttQcnBeta",
                          "Multiplicative decrease when cwnd < mss",
                          DoubleValue(0.25),
                          MakeDoubleAccessor(&RdmaHw::rtt_qcn_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("PowerQcnGradMin",
                          "Minimum gradient of PowerQCN",
                          DoubleValue(-0.2),
                          MakeDoubleAccessor(&RdmaHw::powerqcn_grad_min),
                          MakeDoubleChecker<double>())
            .AddAttribute("PowerQcnGradMax",
                          "Maximum gradient of PowerQCN",
                          DoubleValue(0.6),
                          MakeDoubleAccessor(&RdmaHw::powerqcn_grad_max),
                          MakeDoubleChecker<double>());
    return tid;
}

RdmaHw::RdmaHw()
{
}

void
RdmaHw::SetNode(Ptr<Node> node)
{
    m_node = node;
}

void
RdmaHw::Setup(QpCompleteCallback cb)
{
    for (uint32_t i = 0; i < m_nic.size(); i++)
    {
        Ptr<QbbNetDevice> dev = m_nic[i].dev;
        if (dev == NULL)
        {
            continue;
        }
        // share data with NIC
        dev->m_rdmaEQ->m_qpGrp = m_nic[i].qpGrp;
        // setup callback
        dev->m_rdmaReceiveCb = MakeCallback(&RdmaHw::Receive, this);
        dev->m_rdmaLinkDownCb = MakeCallback(&RdmaHw::SetLinkDown, this);
        dev->m_rdmaPktSent = MakeCallback(&RdmaHw::PktSent, this);
        // config NIC
        dev->m_rdmaEQ->m_rdmaGetNxtPkt = MakeCallback(&RdmaHw::GetNxtPacket, this);
    }
    // setup qp complete callback
    m_qpCompleteCallback = cb;
}

uint32_t
RdmaHw::GetNicIdxOfQp(Ptr<RdmaQueuePair> qp)
{
    auto& v = m_rtTable[qp->dip.Get()];
    if (!v.empty())
    {
        return v[qp->GetHash() % v.size()];
    }
    else
    {
        NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
    }
}

uint64_t
RdmaHw::GetQpKey(uint32_t dip, uint16_t sport, uint16_t pg)
{
    return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg;
}

Ptr<RdmaQueuePair>
RdmaHw::GetQp(uint32_t dip, uint16_t sport, uint16_t pg)
{
    uint64_t key = GetQpKey(dip, sport, pg);
    auto it = m_qpMap.find(key);
    if (it != m_qpMap.end())
    {
        return it->second;
    }
    return NULL;
}

void
RdmaHw::AddQueuePair(uint64_t size,
                     uint16_t pg,
                     Ipv4Address sip,
                     Ipv4Address dip,
                     uint16_t sport,
                     uint16_t dport,
                     uint32_t win,
                     uint64_t baseRtt,
                     Callback<void> notifyAppFinish,
                     Time stopTime)
{
    // create qp
    Ptr<RdmaQueuePair> qp = CreateObject<RdmaQueuePair>(pg, sip, dip, sport, dport);
    qp->SetSize(size);
    qp->SetWin(win);
    qp->SetBaseRtt(baseRtt);
    qp->SetVarWin(m_var_win);
    qp->SetAppNotifyCallback(notifyAppFinish);
    qp->stopTime = stopTime;

    if (stopTime == Simulator::GetMaximumSimulationTime() - MicroSeconds(1))
    {
        qp->incastFlow = 1;
    }
    else
    {
        qp->incastFlow = 0;
    }

    // add qp
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    m_nic[nic_idx].qpGrp->AddQp(qp);
    uint64_t key = GetQpKey(dip.Get(), sport, pg);
    m_qpMap[key] = qp;

    qp->powerEnabled = PowerTCPEnabled;

    // set init variables
    if (m_nic[nic_idx].dev == NULL)
    {
        std::cout << "sip " << sip << " dip " << dip << " sport " << sport << " dport " << dport
                  << std::endl;
    }
    DataRate m_bps = m_nic[nic_idx].dev->GetDataRate();
    if (win)
    {
        qp->SetWin(m_bps.GetBitRate() * 1 * baseRtt * 1e-9 / 8);
    }
    qp->m_rate = m_bps; // transmission starts at full rate
    qp->m_max_rate = m_bps;
    switch (m_cc_mode)
    {
    case CC_MODE::MLX_CNP:
        qp->mlx.m_targetRate = m_bps;
        break;
    case CC_MODE::HPCC:
        qp->hp.m_curRate = m_bps;
        if (m_multipleRate)
        {
            for (uint32_t i = 0; i < IntHeader::maxHop; i++)
            {
                qp->hp.hopState[i].Rc = m_bps;
            }
        }
        break;
    case CC_MODE::PATCHED_TIMELY:
    case CC_MODE::TIMELY:
        qp->tmly.m_curRate = m_bps;
        break;
    case CC_MODE::HPCC_PINT:
        qp->hpccPint.m_curRate = m_bps;
        break;
    case CC_MODE::SWIFT:
        qp->swift.m_curRate = m_bps;
    case CC_MODE::UFCC:
        qp->ufcc.high_rate = m_bps;
        qp->ufcc.low_rate = m_minRate;
        qp->ufcc.high = false;
        qp->ufcc.low = false;
        qp->ufcc.lastRtt = 0;
        qp->ufcc.up = false;
        qp->ufcc.cur_times = 0;
        qp->ufcc.max_times = 4000;
        qp->ufcc.m_lastUpdateSeq = 0;
        qp->ufcc.de_tarRate = 0;
        qp->ufcc.state = qp->INIT;
        qp->ufcc.state_count = 0;
    case CC_MODE::UFCC_CWND:
        qp->ufcc.high_rate = m_bps;
        qp->ufcc.low_rate = m_minRate;
        qp->ufcc.high = false;
        qp->ufcc.low = false;
        qp->ufcc.lastRtt = 0;
        qp->ufcc.up = false;
        qp->ufcc.cur_times = 0;
        qp->ufcc.max_times = 4000;
        qp->ufcc.m_lastUpdateSeq = 0;
        qp->ufcc.de_tarRate = 0;
        qp->ufcc.state = qp->INIT;
        qp->ufcc.state_count = 0;
    }


    // Notify Nic
    m_nic[nic_idx].dev->NewQp(qp);
}

void
RdmaHw::DeleteQueuePair(Ptr<RdmaQueuePair> qp)
{
    // remove qp from the m_qpMap
    uint64_t key = GetQpKey(qp->dip.Get(), qp->sport, qp->m_pg);
    m_qpMap.erase(key);
}

Ptr<RdmaRxQueuePair>
RdmaHw::GetRxQp(uint32_t sip,
                uint32_t dip,
                uint16_t sport,
                uint16_t dport,
                uint16_t pg,
                bool create)
{
    uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
    auto it = m_rxQpMap.find(key);
    if (it != m_rxQpMap.end())
    {
        return it->second;
    }
    if (create)
    {
        // create new rx qp
        Ptr<RdmaRxQueuePair> q = CreateObject<RdmaRxQueuePair>();
        // init the qp
        q->sip = sip;
        q->dip = dip;
        q->sport = sport;
        q->dport = dport;
        q->m_ecn_source.qIndex = pg;
        // store in map
        m_rxQpMap[key] = q;
        return q;
    }
    return NULL;
}

uint32_t
RdmaHw::GetNicIdxOfRxQp(Ptr<RdmaRxQueuePair> q)
{
    auto& v = m_rtTable[q->dip];
    if (!v.empty())
    {
        return v[q->GetHash() % v.size()];
    }
    else
    {
        NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
    }
}

void
RdmaHw::DeleteRxQp(uint32_t dip, uint16_t pg, uint16_t dport)
{
    uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
    m_rxQpMap.erase(key);
}

int
RdmaHw::ReceiveUdp(Ptr<Packet> p, CustomHeader& ch)
{
    uint8_t ecnbits = ch.GetIpv4EcnBits();

    uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();

    // TODO find corresponding rx queue pair
    Ptr<RdmaRxQueuePair> rxQp =
        GetRxQp(ch.dip, ch.sip, ch.udp.dport, ch.udp.sport, ch.udp.pg, true);
    if (ecnbits != 0)
    {
        rxQp->m_ecn_source.ecnbits |= ecnbits;
        rxQp->m_ecn_source.qfb++;
    }
    rxQp->m_ecn_source.total++;
    rxQp->m_milestone_rx = m_ack_interval;

    int x = ReceiverCheckSeq(ch.udp.seq, rxQp, payload_size);
    if (x == 1 || x == 2)
    { // generate ACK or NACK
        qbbHeader seqh;
        seqh.SetSeq(rxQp->ReceiverNextExpectedSeq);
        seqh.SetPG(ch.udp.pg);
        seqh.SetSport(ch.udp.dport);
        seqh.SetDport(ch.udp.sport);
        seqh.SetIntHeader(ch.udp.ih); // ACK will preserve the INT header from the UDP
        if (ecnbits)
        {
            seqh.SetCnp();
        }

        Ptr<Packet> newp =
            Create<Packet>(std::max(60 - 14 - 20 - (int)seqh.GetSerializedSize(), 0));
        newp->AddHeader(seqh);

        Ipv4Header head; // Prepare IPv4 header
        head.SetDestination(Ipv4Address(ch.sip));
        head.SetSource(Ipv4Address(ch.dip));
        head.SetProtocol(x == 1 ? 0xFC : 0xFD); // ack=0xFC nack=0xFD
        head.SetTtl(64);
        head.SetPayloadSize(newp->GetSize());
        head.SetIdentification(rxQp->m_ipid++);

        newp->AddHeader(head);
        AddHeader(newp, 0x800); // Attach PPP header
        // send
        uint32_t nic_idx = GetNicIdxOfRxQp(rxQp);
        m_nic[nic_idx].dev->RdmaEnqueueHighPrioQ(newp);
        m_nic[nic_idx].dev->TriggerTransmit();
    }
    return 0;
}

int
RdmaHw::ReceiveCnp(Ptr<Packet> p, CustomHeader& ch)
{
    // QCN on NIC
    // This is a Congestion signal
    // Then, extract data from the congestion packet.
    // We assume, without verify, the packet is destinated to me
    uint32_t qIndex = ch.cnp.qIndex;
    if (qIndex == 1)
    { // DCTCP
       // std::cout << "TCP--ignore\n";
        return 0;
    }
    uint16_t udpport = ch.cnp.fid; // corresponds to the sport
    uint8_t ecnbits = ch.cnp.ecnBits;
    uint16_t qfb = ch.cnp.qfb;
    uint16_t total = ch.cnp.total;

    uint32_t i;
    // get qp
    Ptr<RdmaQueuePair> qp = GetQp(ch.sip, udpport, qIndex);
    if (qp == NULL)
    {
        //std::cout << "ERROR: QCN NIC cannot find the flow\n";
    }
    // get nic
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;

    if (qp->m_rate == 0) // lazy initialization
    {
        qp->m_rate = dev->GetDataRate();
        if (m_cc_mode == CC_MODE::MLX_CNP)
        {
            qp->mlx.m_targetRate = dev->GetDataRate();
        }
        else if (m_cc_mode == CC_MODE::HPCC)
        {
            qp->hp.m_curRate = dev->GetDataRate();
            if (m_multipleRate)
            {
                for (uint32_t i = 0; i < IntHeader::maxHop; i++)
                {
                    qp->hp.hopState[i].Rc = dev->GetDataRate();
                }
            }
        }
        else if (m_cc_mode == CC_MODE::TIMELY || m_cc_mode == CC_MODE::PATCHED_TIMELY)
        {
            qp->tmly.m_curRate = dev->GetDataRate();
        }
        else if (m_cc_mode == CC_MODE::HPCC_PINT)
        {
            qp->hpccPint.m_curRate = dev->GetDataRate();
        }
        else if (m_cc_mode == CC_MODE::SWIFT)
        {
            qp->swift.m_curRate = dev->GetDataRate();
        }
    }
    return 0;
}

int
RdmaHw::ReceiveAck(Ptr<Packet> p, CustomHeader& ch)
{
    uint16_t qIndex = ch.ack.pg;
    uint16_t port = ch.ack.dport;
    uint32_t seq = ch.ack.seq;
    uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
    int i;
    Ptr<RdmaQueuePair> qp = GetQp(ch.sip, port, qIndex);
    if (qp == NULL)
    {
       // std::cout << "ERROR: "
                  //<< "node:" << m_node->GetId() << ' ' << (ch.l3Prot == 0xFC ? "ACK" : "NACK")
                 // << " NIC cannot find the flow\n";
        return 0;
    }

    uint32_t nic_idx = GetNicIdxOfQp(qp);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
    if (m_ack_interval == 0)
    {
       // std::cout << "ERROR: shouldn't receive ack\n";
    }
    else
    {
        if (!m_backto0)
        {
            qp->Acknowledge(seq);
        }
        else
        {
            uint32_t goback_seq = seq / m_chunk * m_chunk;
            qp->Acknowledge(goback_seq);
        }
        if (qp->IsFinished())
        {
            QpComplete(qp);
        }
    }
    if (ch.l3Prot == 0xFD)
    { // NACK
        RecoverQueue(qp);
    }

    // handle cnp
    if (cnp)
    {
        if (m_cc_mode == CC_MODE::MLX_CNP)
        { // mlx version
            cnp_received_mlx(qp);
        }
    }

    switch (m_cc_mode)
    {
    case CC_MODE::HPCC:
        // Not only HPCC, but also PowerTCP!
        HandleAckHp(qp, p, ch);
        break;
    case CC_MODE::TIMELY:
        HandleAckTimely(qp, p, ch);
        break;
    case CC_MODE::UFCC:
        HandleAckUfcc(qp, p, ch);
        break;
    case CC_MODE::UFCC_CWND:
        HandleAckUfcwnd(qp, p, ch);

    case CC_MODE::DCTCP:
        HandleAckDctcp(qp, p, ch);
        break;
    case CC_MODE::HPCC_PINT:
        HandleAckHpPint(qp, p, ch);
        break;
    case CC_MODE::PATCHED_TIMELY:
        HandleAckPatchedTimely(qp, p, ch);
        break;
    case CC_MODE::SWIFT:
        HandleAckSwift(qp, p, ch);
        break;
    case CC_MODE::RTT_QCN:
        HandleAckRttQcn(qp, p, ch);
        break;
    case CC_MODE::POWERQCN:
        HandleAckPowerQcn(qp, p, ch);
        break;
    default:
        NS_ABORT_MSG("Unknown CC mode");
        break;
    }
    // ACK may advance the on-the-fly window, allowing more packets to send
    dev->TriggerTransmit();
    return 0;
}

int
RdmaHw::Receive(Ptr<Packet> p, CustomHeader& ch)
{


    if (ch.l3Prot == 0x11)
    { // UDP
        ReceiveUdp(p, ch);
    }
    else if (ch.l3Prot == 0xFF)
    { // CNP
        ReceiveCnp(p, ch);
    }
    else if (ch.l3Prot == 0xFD)
    { // NACK
        ReceiveAck(p, ch);
    }
    else if (ch.l3Prot == 0xFC)
    { // ACK
        ReceiveAck(p, ch);
    }
    return 0;


}

int
RdmaHw::ReceiverCheckSeq(uint32_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size) const
{
    uint32_t expected = q->ReceiverNextExpectedSeq;
    if (seq == expected)
    {
        q->ReceiverNextExpectedSeq = expected + size;
        if (q->ReceiverNextExpectedSeq >= q->m_milestone_rx)
        {
            q->m_milestone_rx += m_ack_interval;
            return 1; // Generate ACK
        }
        else if (q->ReceiverNextExpectedSeq % m_chunk == 0)
        {
            return 1;
        }
        else
        {
            return 5;
        }
    }
    else if (seq > expected)
    {
        // Generate NACK
        if (Simulator::Now() >= q->m_nackTimer || q->m_lastNACK != expected)
        {
            q->m_nackTimer = Simulator::Now() + MicroSeconds(m_nack_interval);
            q->m_lastNACK = expected;
            if (m_backto0)
            {
                q->ReceiverNextExpectedSeq = q->ReceiverNextExpectedSeq / m_chunk * m_chunk;
            }
            return 2;
        }
        else
        {
            return 4;
        }
    }
    else
    {
        // Duplicate.
        return 3;
    }
}

void
RdmaHw::AddHeader(Ptr<Packet> p, uint16_t protocolNumber)
{
    PppHeader ppp;
    ppp.SetProtocol(EtherToPpp(protocolNumber));
    p->AddHeader(ppp);
}

uint16_t
RdmaHw::EtherToPpp(uint16_t proto)
{
    switch (proto)
    {
    case 0x0800:
        return 0x0021; // IPv4
    case 0x86DD:
        return 0x0057; // IPv6
    default:
        NS_ASSERT_MSG(false, "PPP Protocol number not defined!");
    }
    return 0;
}

void
RdmaHw::RecoverQueue(Ptr<RdmaQueuePair> qp)
{
    qp->snd_nxt = qp->snd_una;
}

void
RdmaHw::QpComplete(Ptr<RdmaQueuePair> qp)
{
    NS_ASSERT(!m_qpCompleteCallback.IsNull());
    if (m_cc_mode == CC_MODE::MLX_CNP)
    {
        Simulator::Cancel(qp->mlx.m_eventUpdateAlpha);
        Simulator::Cancel(qp->mlx.m_eventDecreaseRate);
        Simulator::Cancel(qp->mlx.m_rpTimer);
    }

    // This callback will log info
    // It may also delete the rxQp on the receiver
    m_qpCompleteCallback(qp);

    qp->m_notifyAppFinish();

    // delete the qp
    DeleteQueuePair(qp);
}

void
RdmaHw::SetLinkDown(Ptr<QbbNetDevice> dev)
{
    printf("RdmaHw: node:%u a link down\n", m_node->GetId());
}

void
RdmaHw::AddTableEntry(Ipv4Address& dstAddr, uint32_t intf_idx)
{
    uint32_t dip = dstAddr.Get();
    m_rtTable[dip].push_back(intf_idx);
}

void
RdmaHw::ClearTable()
{
    m_rtTable.clear();
}

void
RdmaHw::RedistributeQp()
{
    // clear old qpGrp
    for (uint32_t i = 0; i < m_nic.size(); i++)
    {
        if (m_nic[i].dev == NULL)
        {
            continue;
        }
        m_nic[i].qpGrp->Clear();
    }

    // redistribute qp
    for (auto& it : m_qpMap)
    {
        Ptr<RdmaQueuePair> qp = it.second;
        uint32_t nic_idx = GetNicIdxOfQp(qp);
        m_nic[nic_idx].qpGrp->AddQp(qp);
        // Notify Nic
        m_nic[nic_idx].dev->ReassignedQp(qp);
    }
}

Ptr<Packet>
RdmaHw::GetNxtPacket(Ptr<RdmaQueuePair> qp)
{
    uint32_t payload_size = qp->GetBytesLeft();
    if (m_mtu < payload_size)
    {
        payload_size = m_mtu;
    }
    Ptr<Packet> p = Create<Packet>(payload_size);
    uint32_t sentBytes = qp->m_size - qp->GetBytesLeft();
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    DataRate m_bps = m_nic[nic_idx].dev->GetDataRate();
    double bdp = m_bps.GetBitRate() * 1 * qp->m_baseRtt * 1e-9 / 8;
    UnSchedTag unschedtag;
    if (sentBytes <= bdp)
    {
        unschedtag.SetValue(1);
    }
    else
    {
        unschedtag.SetValue(0);
    }
    p->AddPacketTag(unschedtag);
    // add SeqTsHeader
    SeqTsHeader seqTs;
    seqTs.SetSeq(qp->snd_nxt);
    seqTs.SetPG(qp->m_pg);
    p->AddHeader(seqTs);
    // add udp header
    UdpHeader udpHeader;
    udpHeader.SetDestinationPort(qp->dport);
    udpHeader.SetSourcePort(qp->sport);
    p->AddHeader(udpHeader);
    // add ipv4 header
    Ipv4Header ipHeader;
    ipHeader.SetSource(qp->sip);
    ipHeader.SetDestination(qp->dip);
    ipHeader.SetProtocol(0x11);
    ipHeader.SetPayloadSize(p->GetSize());
    ipHeader.SetTtl(64);
    ipHeader.SetTos(0);
    ipHeader.SetIdentification(qp->m_ipid);
    p->AddHeader(ipHeader);
    // add ppp header
    PppHeader ppp;
    ppp.SetProtocol(0x0021); // EtherToPpp(0x800), see point-to-point-net-device.cc
    p->AddHeader(ppp);

    // update state
    qp->snd_nxt += payload_size;
    qp->m_ipid++;

    // return
    return p;
}

void
RdmaHw::PktSent(Ptr<RdmaQueuePair> qp, Ptr<Packet> pkt, Time interframeGap) const
{
    qp->lastPktSize = pkt->GetSize();
    //	SeqTsHeader seqTs;
    //	pkt->PeekHeader(seqTs);
    uint32_t seq = qp->snd_nxt;
    qp->rates[qp->snd_nxt] = Simulator::Now().GetNanoSeconds();
    UpdateNextAvail(qp, interframeGap, pkt->GetSize());
}

void
RdmaHw::UpdateNextAvail(Ptr<RdmaQueuePair> qp, Time interframeGap, uint32_t pkt_size) const
{
    Time sendingTime;
    if (m_rateBound)
    {
        sendingTime = interframeGap + qp->m_rate.CalculateBytesTxTime(pkt_size);
    }
    else
    {
        sendingTime = interframeGap + qp->m_max_rate.CalculateBytesTxTime(pkt_size);
    }
    qp->m_nextAvail = std::max(Simulator::Now() + sendingTime, qp->m_nextAvail);
}

void
RdmaHw::ChangeRate(Ptr<RdmaQueuePair> qp, DataRate new_rate)
{
#if 1
    Time sendingTime = qp->m_rate.CalculateBytesTxTime(qp->lastPktSize);
    Time new_sendintTime = new_rate.CalculateBytesTxTime(qp->lastPktSize);
    qp->m_nextAvail = qp->m_nextAvail + new_sendintTime - sendingTime;
    // update nic's next avail event
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    m_nic[nic_idx].dev->UpdateNextAvail(qp->m_nextAvail);
#endif
    // change to new rate
    qp->m_rate = new_rate;
}

#define PRINT_LOG 0

/******************************
 * Mellanox's version of DCQCN
 *****************************/
void
RdmaHw::UpdateAlphaMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    // std::cout << Simulator::Now() << " alpha update:" << m_node->GetId() << ' ' << q->mlx.m_alpha
    // << ' ' << (int)q->mlx.m_alpha_cnp_arrived << '\n'; printf("%lu alpha update: %08x %08x %u %u
    // %.6lf->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport,
    // q->mlx.m_alpha);
#endif
    if (q->mlx.m_alpha_cnp_arrived)
    {
        q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha + m_g; // binary feedback
    }
    else
    {
        q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha; // binary feedback
    }
#if PRINT_LOG
    // printf("%.6lf\n", q->mlx.m_alpha);
#endif
    q->mlx.m_alpha_cnp_arrived = false; // clear the CNP_arrived bit
    ScheduleUpdateAlphaMlx(q);
}

void
RdmaHw::ScheduleUpdateAlphaMlx(Ptr<RdmaQueuePair> q)
{
    q->mlx.m_eventUpdateAlpha = Simulator::Schedule(MicroSeconds(m_alpha_resume_interval),
                                                    &RdmaHw::UpdateAlphaMlx,
                                                    this,
                                                    q);
}

void
RdmaHw::cnp_received_mlx(Ptr<RdmaQueuePair> q)
{
    q->mlx.m_alpha_cnp_arrived = true;    // set CNP_arrived bit for alpha update
    q->mlx.m_decrease_cnp_arrived = true; // set CNP_arrived bit for rate decrease
    if (q->mlx.m_first_cnp)
    {
        // init alpha
        q->mlx.m_alpha = 1;
        q->mlx.m_alpha_cnp_arrived = false;
        // schedule alpha update
        ScheduleUpdateAlphaMlx(q);
        // schedule rate decrease
        ScheduleDecreaseRateMlx(q, 1); // add 1 ns to make sure rate decrease is after alpha update
        // set rate on first CNP
        q->mlx.m_targetRate = q->m_rate = m_rateOnFirstCNP * q->m_rate;
        q->mlx.m_first_cnp = false;
    }
}

void
RdmaHw::CheckRateDecreaseMlx(Ptr<RdmaQueuePair> q)
{
    ScheduleDecreaseRateMlx(q, 0);
    if (q->mlx.m_decrease_cnp_arrived)
    {
#if PRINT_LOG
        printf("%lu rate dec: %08x %08x %u %u (%0.3lf %.3lf)->",
               Simulator::Now().GetTimeStep(),
               q->sip.Get(),
               q->dip.Get(),
               q->sport,
               q->dport,
               q->mlx.m_targetRate.GetBitRate() * 1e-9,
               q->m_rate.GetBitRate() * 1e-9);
#endif
        bool clamp = true;
        if (!m_EcnClampTgtRate)
        {
            if (q->mlx.m_rpTimeStage == 0)
            {
                clamp = false;
            }
        }
        if (clamp)
        {
            q->mlx.m_targetRate = q->m_rate;
        }
        q->m_rate = std::max(m_minRate, q->m_rate * (1 - q->mlx.m_alpha / 2));
        // reset rate increase related things
        q->mlx.m_rpTimeStage = 0;
        q->mlx.m_decrease_cnp_arrived = false;
        Simulator::Cancel(q->mlx.m_rpTimer);
        q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset),
                                               &RdmaHw::RateIncEventTimerMlx,
                                               this,
                                               q);
#if PRINT_LOG
        printf("(%.3lf %.3lf)\n",
               q->mlx.m_targetRate.GetBitRate() * 1e-9,
               q->m_rate.GetBitRate() * 1e-9);
#endif
    }
}

void
RdmaHw::ScheduleDecreaseRateMlx(Ptr<RdmaQueuePair> q, uint32_t delta)
{
    q->mlx.m_eventDecreaseRate =
        Simulator::Schedule(MicroSeconds(m_rateDecreaseInterval) + NanoSeconds(delta),
                            &RdmaHw::CheckRateDecreaseMlx,
                            this,
                            q);
}

void
RdmaHw::RateIncEventTimerMlx(Ptr<RdmaQueuePair> q)
{
    q->mlx.m_rpTimer =
        Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
    RateIncEventMlx(q);
    q->mlx.m_rpTimeStage++;
}

void
RdmaHw::RateIncEventMlx(Ptr<RdmaQueuePair> q)
{
    // check which increase phase: fast recovery, active increase, hyper increase
    if (q->mlx.m_rpTimeStage < m_rpgThreshold)
    { // fast recovery
        FastRecoveryMlx(q);
    }
    else if (q->mlx.m_rpTimeStage == m_rpgThreshold)
    { // active increase
        ActiveIncreaseMlx(q);
    }
    else
    { // hyper increase
        HyperIncreaseMlx(q);
    }
}

void
RdmaHw::FastRecoveryMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    printf("%lu fast recovery: %08x %08x %u %u (%0.3lf %.3lf)->",
           Simulator::Now().GetTimeStep(),
           q->sip.Get(),
           q->dip.Get(),
           q->sport,
           q->dport,
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
    q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
    printf("(%.3lf %.3lf)\n",
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
}

void
RdmaHw::ActiveIncreaseMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    printf("%lu active inc: %08x %08x %u %u (%0.3lf %.3lf)->",
           Simulator::Now().GetTimeStep(),
           q->sip.Get(),
           q->dip.Get(),
           q->sport,
           q->dport,
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
    // get NIC
    uint32_t nic_idx = GetNicIdxOfQp(q);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
    // increate rate
    q->mlx.m_targetRate += m_rai;
    if (q->mlx.m_targetRate > dev->GetDataRate())
    {
        q->mlx.m_targetRate = dev->GetDataRate();
    }
    q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
    printf("(%.3lf %.3lf)\n",
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
}

void
RdmaHw::HyperIncreaseMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    printf("%lu hyper inc: %08x %08x %u %u (%0.3lf %.3lf)->",
           Simulator::Now().GetTimeStep(),
           q->sip.Get(),
           q->dip.Get(),
           q->sport,
           q->dport,
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
    // get NIC
    uint32_t nic_idx = GetNicIdxOfQp(q);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
    // increate rate
    q->mlx.m_targetRate += m_rhai;
    if (q->mlx.m_targetRate > dev->GetDataRate())
    {
        q->mlx.m_targetRate = dev->GetDataRate();
    }
    q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
    printf("(%.3lf %.3lf)\n",
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
}

/***********************
 * High Precision CC
 ***********************/
void
RdmaHw::HandleAckHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    if (ack_seq > qp->hp.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        if (PowerTCPEnabled)
        {
            UpdateRatePower(qp, p, ch, false);
        }
        else
        {
            UpdateRateHp(qp, p, ch, false);
        }
    }
    else
    { // do fast react
        if (PowerTCPEnabled)
        {
            FastReactPower(qp, p, ch);
        }
        else
        {
            FastReactHp(qp, p, ch);
        }
    }
}

void
RdmaHw::UpdateRateHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react)
{
    uint32_t next_seq = qp->snd_nxt;
    bool print = true;

    if (qp->hp.m_lastUpdateSeq == 0)
    { // first RTT

        qp->hp.m_lastUpdateSeq = next_seq;
        // store INT
        IntHeader& ih = ch.ack.ih;
        NS_ASSERT(ih.nhop <= IntHeader::maxHop);
        for (uint32_t i = 0; i < ih.nhop; i++)
        {
            qp->hp.hop[i] = ih.hop[i];
        }
#if PRINT_LOG
        if (print)
        {
            printf("%lu %s %08x %08x %u %u [%u,%u,%u]",
                   Simulator::Now().GetTimeStep(),
                   fast_react ? "fast" : "update",
                   qp->sip.Get(),
                   qp->dip.Get(),
                   qp->sport,
                   qp->dport,
                   qp->hp.m_lastUpdateSeq,
                   ch.ack.seq,
                   next_seq);
            for (uint32_t i = 0; i < ih.nhop; i++)
                printf(" %u %lu %lu",
                       ih.hop[i].GetQlen(),
                       ih.hop[i].GetBytes(),
                       ih.hop[i].GetTime());
            printf("\n");
        }
#endif
    }
    else
    {
        // check packet INT
        IntHeader& ih = ch.ack.ih;
        if (ih.nhop <= IntHeader::maxHop)
        {
            double max_c = 0;
            bool inStable = false;
#if PRINT_LOG
            if (print)
                printf("%lu %s %08x %08x %u %u [%u,%u,%u]",
                       Simulator::Now().GetTimeStep(),
                       fast_react ? "fast" : "update",
                       qp->sip.Get(),
                       qp->dip.Get(),
                       qp->sport,
                       qp->dport,
                       qp->hp.m_lastUpdateSeq,
                       ch.ack.seq,
                       next_seq);
#endif
            // check each hop
            double U = 0;
            uint64_t dt = 0;
            bool updated[IntHeader::maxHop] = {false};
            bool updated_any = false;
            NS_ASSERT(ih.nhop <= IntHeader::maxHop);
            for (uint32_t i = 0; i < ih.nhop; i++)
            {
                if (m_sampleFeedback)
                {
                    if (ih.hop[i].GetQlen() == 0 and fast_react)
                    {
                        continue;
                    }
                }
                updated[i] = updated_any = true;
#if PRINT_LOG
                if (print)
                    printf(" %u(%u) %lu(%lu) %lu(%lu)",
                           ih.hop[i].GetQlen(),
                           qp->hp.hop[i].GetQlen(),
                           ih.hop[i].GetBytes(),
                           qp->hp.hop[i].GetBytes(),
                           ih.hop[i].GetTime(),
                           qp->hp.hop[i].GetTime());
#endif
                uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);
                double duration = tau * 1e-9;
                double txRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8 / duration;

                double u;
                u = txRate / ih.hop[i].GetLineRate() +
                    (double)std::min(ih.hop[i].GetQlen(), qp->hp.hop[i].GetQlen()) *
                        qp->m_max_rate.GetBitRate() / ih.hop[i].GetLineRate() / qp->m_win;

#if PRINT_LOG
                if (print)
                    printf(" %.3lf %.3lf", txRate, u);
#endif
                if (!m_multipleRate)
                {
                    // for aggregate (single R)
                    if (u > U)
                    {
                        U = u;
                        dt = tau;
                    }
                }
                else
                {
                    // for per hop (per hop R)
                    if (tau > qp->m_baseRtt)
                    {
                        tau = qp->m_baseRtt;
                    }
                    qp->hp.hopState[i].u =
                        (qp->hp.hopState[i].u * (qp->m_baseRtt - tau) + u * tau) /
                        double(qp->m_baseRtt);
                }
                qp->hp.hop[i] = ih.hop[i];
            }

            DataRate new_rate;
            int32_t new_incStage;
            DataRate new_rate_per_hop[IntHeader::maxHop];
            int32_t new_incStage_per_hop[IntHeader::maxHop];
            if (!m_multipleRate)
            {
                // for aggregate (single R)
                if (updated_any)
                {
                    if (dt > 1.0 * qp->m_baseRtt)
                    {
                        dt = 1.0 * qp->m_baseRtt;
                    }

                    qp->hp.u = (qp->hp.u * (qp->m_baseRtt - dt) + U * dt) / double(qp->m_baseRtt);
                    max_c = qp->hp.u / m_targetUtil;

                    if (max_c >= 1 || qp->hp.m_incStage >= m_miThresh)
                    {
                        new_rate = qp->hp.m_curRate / max_c + m_rai;
                        new_incStage = 0;
                    }
                    else
                    {
                        new_rate = qp->hp.m_curRate + m_rai;
                        new_incStage = qp->hp.m_incStage + 1;
                    }

                    if (new_rate < m_minRate)
                    {
                        new_rate = m_minRate;
                    }
                    if (new_rate > qp->m_max_rate)
                    {
                        new_rate = qp->m_max_rate;
                    }
#if PRINT_LOG
                    if (print)
                        printf(" u=%.6lf U=%.3lf dt=%u max_c=%.3lf", qp->hp.u, U, dt, max_c);
#endif
#if PRINT_LOG
                    if (print)
                        printf(" rate:%.3lf->%.3lf\n",
                               qp->hp.m_curRate.GetBitRate() * 1e-9,
                               new_rate.GetBitRate() * 1e-9);
#endif
                }
            }
            else
            {
                // for per hop (per hop R)
                new_rate = qp->m_max_rate;
                for (uint32_t i = 0; i < ih.nhop; i++)
                {
                    if (updated[i])
                    {
                        double c = qp->hp.hopState[i].u / m_targetUtil;
                        if (c >= 1 || qp->hp.hopState[i].incStage >= m_miThresh)
                        {
                            new_rate_per_hop[i] = qp->hp.hopState[i].Rc / c + m_rai;
                            new_incStage_per_hop[i] = 0;
                        }
                        else
                        {
                            new_rate_per_hop[i] = qp->hp.hopState[i].Rc + m_rai;
                            new_incStage_per_hop[i] = qp->hp.hopState[i].incStage + 1;
                        }
                        // bound rate
                        if (new_rate_per_hop[i] < m_minRate)
                        {
                            new_rate_per_hop[i] = m_minRate;
                        }
                        if (new_rate_per_hop[i] > qp->m_max_rate)
                        {
                            new_rate_per_hop[i] = qp->m_max_rate;
                        }
                        // find min new_rate
                        if (new_rate_per_hop[i] < new_rate)
                        {
                            new_rate = new_rate_per_hop[i];
                        }
#if PRINT_LOG
                        if (print)
                            printf(" [%u]u=%.6lf c=%.3lf", i, qp->hp.hopState[i].u, c);
#endif
#if PRINT_LOG
                        if (print)
                            printf(" %.3lf->%.3lf",
                                   qp->hp.hopState[i].Rc.GetBitRate() * 1e-9,
                                   new_rate.GetBitRate() * 1e-9);
#endif
                    }
                    else
                    {
                        if (qp->hp.hopState[i].Rc < new_rate)
                        {
                            new_rate = qp->hp.hopState[i].Rc;
                        }
                    }
                }
#if PRINT_LOG
                printf("\n");
#endif
            }

            if (updated_any)
            {
                ChangeRate(qp, new_rate);
            }
            if (!fast_react)
            {
                if (updated_any)
                {
                    qp->hp.m_curRate = new_rate;
                    qp->hp.m_incStage = new_incStage;
                }
                if (m_multipleRate)
                {
                    // for per hop (per hop R)
                    for (uint32_t i = 0; i < ih.nhop; i++)
                    {
                        if (updated[i])
                        {
                            qp->hp.hopState[i].Rc = new_rate_per_hop[i];
                            qp->hp.hopState[i].incStage = new_incStage_per_hop[i];
                        }
                    }
                }
            }
        }
        if (!fast_react)
        {
            if (next_seq > qp->hp.m_lastUpdateSeq)
            {
                qp->hp.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
            }
        }
    }
}

void
RdmaHw::FastReactHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    if (m_fast_react)
    {
        UpdateRateHp(qp, p, ch, true);
    }
}

/**********************
 * PowerTCP (Int/Delay versions) called from HandleAckHp function at the moment
 *********************/

void
RdmaHw::UpdateRatePower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react)
{
    uint32_t next_seq = qp->snd_nxt;
    bool print = true;
    double prevRtt = qp->m_baseRtt;
    double prevCompletion = Simulator::Now().GetNanoSeconds();
    std::map<uint32_t, double>::iterator it = qp->rates.find(ch.ack.seq);
    DataRate old;
    double rtt;

    if (it != qp->rates.end())
    {
        qp->rates.erase(it);
        prevRtt = Simulator::Now().GetNanoSeconds() - it->second;
        if (PowerTCPdelay)
        {
            qp->m_baseRtt =
                std::min(uint64_t(Simulator::Now().GetNanoSeconds() - it->second), qp->m_baseRtt);
        }
        prevCompletion = Simulator::Now().GetNanoSeconds();
    }
    if (qp->hp.m_lastUpdateSeq == 0 && !PowerTCPdelay)
    {
        qp->prevRtt = prevRtt;
        qp->prevCompletion = Simulator::Now().GetNanoSeconds();
        qp->hp.m_lastUpdateSeq = next_seq;
        // store INT
        IntHeader& ih = ch.ack.ih;
        NS_ASSERT(ih.nhop <= IntHeader::maxHop);
        for (uint32_t i = 0; i < ih.nhop; i++)
        {
            qp->hp.hop[i] = ih.hop[i];
        }
    }
    else
    {
        // check packet INT
        IntHeader& ih = ch.ack.ih;
        if (ih.nhop <= IntHeader::maxHop)
        {
            double max_c = 0;
            bool inStable = false;
            // check each hop
            double U = 0;
            uint64_t dt = 0;
            bool updated[IntHeader::maxHop] = {false};
            bool updated_any = false;
            NS_ASSERT(ih.nhop <= IntHeader::maxHop);
            for (uint32_t i = 0; i < ih.nhop; i++)
            {
                if (m_sampleFeedback)
                {
                    if (ih.hop[i].GetQlen() == 0 and fast_react)
                    {
                        continue;
                    }
                }
                updated[i] = updated_any = true;

                uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);
                double duration = tau * 1e-9;
                double rxRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8.0 / duration;

                double u;

                if (!PowerTCPdelay)
                {
                    double A = rxRate;
                    // double A = txRate + (double(ih.hop[i].GetQlen() * 8.0) -
                    // double(qp->hp.hop[i].GetQlen() * 8.0)) / duration;
                    double power = (A) * (double(ih.hop[i].GetQlen() * 8.0) +
                                          ih.hop[i].GetLineRate() * (qp->m_baseRtt * 1e-9));
                    double powerx = (power) / (ih.hop[i].GetLineRate() *
                                               (ih.hop[i].GetLineRate() * qp->m_baseRtt * 1e-9));
                    u = powerx; // PowerTCP
                }
                else
                {
                    // delay approach
                    double A =
                        (double(prevRtt - qp->prevRtt) / (prevCompletion - qp->prevCompletion) + 1);
                    if (A < 0.5)
                    {
                        A = 0.5;
                    }
                    double power = (A) * (prevRtt);
                    double powerx = (power) / (1.05 * qp->m_baseRtt);
                    u = powerx; // theta-PowerTCP
                }
                if (u > U)
                {
                    U = u;
                    if (PowerTCPdelay)
                    {
                        dt = prevCompletion - qp->prevCompletion;
                    }
                    else
                    {
                        dt = tau;
                    }
                }
                qp->hp.hop[i] = ih.hop[i];
            }

            DataRate new_rate;
            int32_t new_incStage = 0;
            DataRate new_rate_per_hop[IntHeader::maxHop];
            int32_t new_incStage_per_hop[IntHeader::maxHop];

            if (updated_any)
            {
                if (dt > 1.0 * qp->m_baseRtt)
                {
                    dt = 1.0 * qp->m_baseRtt;
                }

                if (U < 0)
                {
                    U = qp->hp.u;
                }
                qp->hp.u =
                    (qp->hp.u * (1.0 * qp->m_baseRtt - dt) + U * dt) / double(1.0 * qp->m_baseRtt);
                if (!PowerTCPdelay)
                {
                    max_c = qp->hp.u / m_targetUtil;
                    new_rate = (0.9 * (qp->hp.m_curRate / max_c + DataRate("150Mbps")) +
                                0.1 * qp->hp.m_curRate); // gamma (EWMA param) = 0.9 for delay
                }
                else
                {
                    max_c = qp->hp.u;
                    new_rate = (0.7 * (qp->hp.m_curRate / max_c + DataRate("150Mbps")) +
                                0.3 * qp->hp.m_curRate); // gamma (EWMA param) = 0.7
                }
                if (new_rate < m_minRate)
                {
                    new_rate = m_minRate;
                }
                if (new_rate > qp->m_max_rate)
                {
                    new_rate = qp->m_max_rate;
                }
            }
            qp->prevRtt = prevRtt;
            qp->prevCompletion = Simulator::Now().GetNanoSeconds();
            if (updated_any)
            {
                ChangeRate(qp, new_rate);
            }
            if (!fast_react)
            {
                if (updated_any)
                {
                    qp->hp.m_curRate = new_rate;
                    qp->hp.m_incStage = new_incStage;
                }
            }
        }
        if (!fast_react)
        {
            if (next_seq > qp->hp.m_lastUpdateSeq)
            {
                qp->hp.m_lastUpdateSeq = next_seq;
            }
        }
    }
}

void
RdmaHw::FastReactPower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    if (m_fast_react)
    {
        UpdateRatePower(qp, p, ch, true);
    }
}

/**********************
 * TIMELY
 *********************/
void
RdmaHw::HandleAckTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    if (ack_seq > qp->tmly.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        UpdateRateTimely(qp, p, ch, false);
    }
    else
    { // do fast react
        FastReactTimely(qp, p, ch);
    }
}

void
RdmaHw::UpdateRateTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us) const
{
    uint32_t next_seq = qp->snd_nxt;
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;
    bool print = !us;
    if (qp->tmly.m_lastUpdateSeq != 0)
    { // not first RTT
        int64_t new_rtt_diff = (int64_t)rtt - (int64_t)qp->tmly.lastRtt;
        double rtt_diff = (1 - m_tmly_alpha) * qp->tmly.rttDiff + m_tmly_alpha * new_rtt_diff;
        double gradient = rtt_diff / m_tmly_minRtt;
        bool inc = false;
        double c = 0;
#if PRINT_LOG
        if (print)
            printf("%lu node:%u rtt:%lu rttDiff:%.0lf gradient:%.3lf rate:%.3lf",
                   Simulator::Now().GetTimeStep(),
                   m_node->GetId(),
                   rtt,
                   rtt_diff,
                   gradient,
                   qp->tmly.m_curRate.GetBitRate() * 1e-9);
#endif
        if (rtt < m_tmly_TLow)
        {
            inc = true;
        }
        else if (rtt > m_tmly_THigh)
        {
            c = 1 - m_tmly_beta * (1 - (double)m_tmly_THigh / rtt);
            inc = false;
        }
        else if (gradient <= 0)
        {
            inc = true;
        }
        else
        {
            c = 1 - m_tmly_beta * gradient;
            if (c < 0)
            {
                c = 0;
            }
            inc = false;
        }
        if (inc)
        {
            if (qp->tmly.m_incStage < 5)
            // WTF does this mean?
            {
                qp->m_rate = qp->tmly.m_curRate + m_rai;
            }
            else
            {
                qp->m_rate = qp->tmly.m_curRate + m_rhai;
            }
            if (qp->m_rate > qp->m_max_rate)
            {
                qp->m_rate = qp->m_max_rate;
            }
            if (!us)
            {
                qp->tmly.m_curRate = qp->m_rate;
                qp->tmly.m_incStage++;
                qp->tmly.rttDiff = rtt_diff;
            }
        }
        else
        {
            qp->m_rate = std::max(m_minRate, qp->tmly.m_curRate * c);
            if (!us)
            {
                qp->tmly.m_curRate = qp->m_rate;
                qp->tmly.m_incStage = 0;
                qp->tmly.rttDiff = rtt_diff;
            }
        }
#if PRINT_LOG
        if (print)
        {
            printf(" %c %.3lf\n", inc ? '^' : 'v', qp->m_rate.GetBitRate() * 1e-9);
        }
#endif
    }
    if (!us && next_seq > qp->tmly.m_lastUpdateSeq)
    {
        qp->tmly.m_lastUpdateSeq = next_seq;
        // update
        qp->tmly.lastRtt = rtt;
    }
}

void
RdmaHw::FastReactTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
}


/**********************
 * UFCC
 *********************/
void
RdmaHw::HandleAckUfcc(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    //printf("%lu node:%u ack_num: %lu last_ack_num: %lu\n" ,Simulator::Now().GetTimeStep(),m_node->GetId(), ack_seq, qp->ufcc.m_lastUpdateSeq);
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;
    //qp->txBytes
    if(qp->ufcc.lastRtt == 0)
    {
        qp->ufcc.arvgRtt = rtt;
        uint64_t minRtt = m_tmly_minRtt;
        qp->ufcc.minRtt = std::min(rtt , minRtt);
        qp->ufcc.lastRtt = rtt;
        qp->ufcc.m_lastUpdateSeq = qp->snd_nxt;
        return;
    }

    qp->ufcc.minRtt = std::min(rtt,qp->ufcc.minRtt);
    
        
    if(rtt>qp->ufcc.minRtt + burst_rtt)
    {
        qp->ufcc.state = qp->BURST;
    }

    if (ack_seq > qp->ufcc.m_lastUpdateSeq )
    { // if full RTT feedback is ready, do full update

        UpdateRateUfcc(qp, p, ch, false);


    }else if(ack_seq <= qp->ufcc.m_lastUpdateSeq)
    {
         // do fast react


        if(qp->ufcc.state == qp->INIT)
        {
   
            if(rtt <= qp->ufcc.lastRtt)
            {
                qp->ufcc.high = false;
                
                qp->m_rate = std::min(qp->m_rate + qp->ufcc.up_rate,qp->ufcc.high_rate );

                    //printf("node:%u rtt_diff: %f up rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);

                    

                    
            }else
            {
                qp->ufcc.low = false;
                qp->m_rate = std::max(qp->m_rate - qp->ufcc.down_rate ,qp->ufcc.low_rate);   
                //printf("node:%u rtt_diff: %f down rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);

                    //("ack:%u down rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", ack_seq, qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);
            }

            //printf("node:%u rtt_diff: %f\n", m_node->GetId(),rtt_diff-1);
        }else if (qp->ufcc.state == qp->STEADY)
        {
            if(rtt <= qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.high_rate;
                //printf("node: %u low rate\n", m_node->GetId());

            }else if(rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate;
              
            }

            if(qp->ufcc.arvgRtt > qp->ufcc.minRtt + high_rtt && rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate*0.95;

            }
            if (qp->ufcc.arvgRtt < qp->ufcc.minRtt + 0.7*(low_rtt + high_rtt))
            {
                qp->m_rate = std::min(qp->m_max_rate,qp->ufcc.high_rate*1.05);

            }


            if( rtt < qp->ufcc.minRtt + 0.25*low_rtt && qp->m_rate != qp->m_max_rate)
            {
                qp->ufcc.state_count++;
            }else
            {
                qp->ufcc.state_count = 0;
            }

            if(qp->ufcc.state_count >= 1)
            {
                qp->ufcc.high_rate = qp->m_max_rate;

                qp->m_rate = qp->ufcc.high_rate;
                qp->ufcc.up_rate = 0;
                qp->ufcc.down_rate = 1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);   
                qp->ufcc.low = false;
                qp->ufcc.high = false;
                
                qp->ufcc.state = qp->INIT;
                qp->ufcc.state_count = 0;
            }

        }else if (qp->ufcc.state == qp->BURST)
        {
            qp->m_rate = std::max(0.3*qp->ufcc.low_rate , m_minRate);
   
            if( rtt <= qp->ufcc.minRtt + burst_rtt)
            {
                qp->m_rate = std::min((0.9*qp->ufcc.low_rate+ qp->ufcc.high_rate)/2, 2*0.9*qp->ufcc.low_rate);
               
            }

        }

    }

    ChangeRate(qp,qp->m_rate);
}

void
RdmaHw::UpdateRateUfcc(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us) const
{
    


    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;



    if (qp->ufcc.lastRtt != 0)
    { // not first RTT


        //printf("%lu node:%u rtt_diff:%f rate:%lu tar_rate:%lu Rtt:%lu\n",Simulator::Now().GetTimeStep(),m_node->GetId(), rtt_diff , qp->m_rate.GetBitRate(),qp->ufcc.m_tarRate.GetBitRate(), rtt);




        if(qp->ufcc.state == qp->INIT)
        {
            if(rtt <= qp->ufcc.lastRtt)
            {
                qp->ufcc.high = false;               
                qp->m_rate = std::min(qp->m_rate + qp->ufcc.up_rate,qp->ufcc.high_rate );
               // printf("node:%u rtt_diff: %f up rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);

                
            }else 
            {
                qp->ufcc.low = false;
                qp->m_rate = std::max(qp->m_rate - qp->ufcc.down_rate ,qp->ufcc.low_rate);   
               // printf("node:%u rtt_diff: %f down rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);
            }

            if(qp->ufcc.low )
            {
                qp->ufcc.low_rate = qp->ufcc.last_rate;
            }
            if(qp->ufcc.high)
            {
                qp->ufcc.high_rate = qp->ufcc.last_rate;
            }

            qp->ufcc.low = true;
            qp->ufcc.high = true;

            if(qp->ufcc.low_rate  >= 0.95*qp->ufcc.high_rate)
            {
                if(qp->ufcc.arvgRtt <= qp->ufcc.minRtt + 0.5*(low_rtt + high_rtt))
            {
                qp->ufcc.state_count = 0;
                qp->ufcc.state = qp->STEADY;
                }else if(qp->ufcc.arvgRtt > qp->ufcc.minRtt + high_rtt)
                {

                    qp->ufcc.state_count = qp->ufcc.state_count+3;
                }else
        {
            qp->ufcc.state_count++;
                }
                if(qp->ufcc.state_count >= 5)
            {
                    qp->m_rate = std::max(qp->ufcc.low_rate - 0.1*m_minRate,m_minRate);
                    qp->ufcc.low_rate = qp->m_rate;
                qp->ufcc.state_count = 0;
                }
                
            }


            qp->m_rate = std::max(qp->ufcc.low_rate , qp->m_rate);
            qp->m_rate = std::min(qp->ufcc.high_rate , qp->m_rate);
            
            qp->ufcc.up_rate = 1000* std::min(0.5*(qp->ufcc.high_rate - qp->m_rate),qp->ufcc.high_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);
            qp->ufcc.down_rate =1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);



        }else if (qp->ufcc.state == qp->STEADY)
        {
            if(rtt <= qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.high_rate;
                //printf("node: %u low rate\n", m_node->GetId());

            }else if(rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate;
                

            }

            if(qp->ufcc.arvgRtt > qp->ufcc.minRtt + high_rtt  && rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate*0.95;

            }
            if (qp->ufcc.arvgRtt < qp->ufcc.minRtt + 0.7*(low_rtt + high_rtt))
            {
                qp->m_rate = std::min(qp->m_max_rate,qp->ufcc.high_rate*1.05);

            }



            if( rtt < qp->ufcc.minRtt + 0.25*low_rtt && qp->m_rate != qp->m_max_rate)
            {
                qp->ufcc.state_count++;
            }else
            {
                qp->ufcc.state_count = 0;
            }

            if(qp->ufcc.state_count >= 1)
            {
                qp->ufcc.high_rate = qp->m_max_rate;

                qp->m_rate = qp->ufcc.high_rate;
                qp->ufcc.up_rate = 0;
                qp->ufcc.down_rate = 1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);   
                qp->ufcc.low = false;
                qp->ufcc.high = false;
                qp->ufcc.state_count = 0;
                qp->ufcc.state = qp->INIT;
            }



        }else if (qp->ufcc.state == qp->RELEASE)
        {
            /* code */
        }else if (qp->ufcc.state == qp->PREEMPT)
        {



        }else if (qp->ufcc.state == qp->BURST)
        {
            if(rtt <=qp->ufcc.minRtt + burst_rtt)
            {
                if(qp->ufcc.low_rate >= 0.8*qp->ufcc.high_rate)
                {
                    qp->ufcc.high_rate =  (qp->m_max_rate+qp->ufcc.high_rate)/2;
            }else
            {
                    qp->ufcc.high_rate = (qp->ufcc.low_rate + qp->ufcc.high_rate)/2;
            }

                qp->ufcc.low_rate = std::max(0.9*qp->ufcc.low_rate, m_minRate);

                qp->m_rate = std::min((qp->ufcc.low_rate+ qp->ufcc.high_rate)/2, 2*qp->ufcc.low_rate);
                qp->ufcc.down_rate =1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);

                qp->ufcc.up_rate = 1000* std::min(0.5*(qp->ufcc.high_rate - qp->m_rate),qp->ufcc.high_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);
                qp->ufcc.low = false;
                qp->ufcc.high = false;
                qp->ufcc.down_rate = 0;
                    qp->ufcc.state_count = 0;
                    qp->ufcc.state = qp->INIT;

            }
        }
        
         qp->ufcc.last_rate = qp->m_rate;  
        //printf("%lu node:%u aveRTT: %lu STATE:%u rate:%lu rtt_diff:%f last_rtt: %lu rtt: %lu low_rate:%lu high_rate:%lu up_rate: %lu down_rate:%lu minRtt:%lu\n",Simulator::Now().GetTimeStep(),m_node->GetId(),qp->ufcc.arvgRtt, qp->ufcc.state, qp->m_rate.GetBitRate(),  rtt_diff , qp->ufcc.lastRtt, rtt,qp->ufcc.low_rate.GetBitRate(), qp->ufcc.high_rate.GetBitRate(),qp->ufcc.up_rate,qp->ufcc.down_rate,qp->ufcc.minRtt);
        qp->ufcc.arvgRtt = 0.3*qp->ufcc.arvgRtt + 0.7*rtt;
        qp->ufcc.m_lastUpdateSeq = qp->snd_nxt;
        qp->ufcc.lastRtt = rtt;
    }

        
        
    


    
}


/**********************
 * UFCC CWND
 *********************/
void
RdmaHw::HandleAckUfcwnd(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    //printf("%lu node:%u ack_num: %lu last_ack_num: %lu\n" ,Simulator::Now().GetTimeStep(),m_node->GetId(), ack_seq, qp->ufcc.m_lastUpdateSeq);
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;
    //qp->txBytes
    if(qp->ufcc.lastRtt == 0)
    {
        qp->ufcc.arvgRtt = rtt;
        uint64_t minRtt = m_tmly_minRtt;
        qp->ufcc.minRtt = std::min(rtt , minRtt);
        qp->ufcc.lastRtt = rtt;
        qp->ufcc.m_lastUpdateSeq = qp->snd_nxt;
        return;
    }

    qp->ufcc.minRtt = std::min(rtt,qp->ufcc.minRtt);
    
        
    if(rtt>qp->ufcc.minRtt + burst_rtt)
    {
        qp->ufcc.state = qp->BURST;
    }

    if (ack_seq > qp->ufcc.m_lastUpdateSeq )
    { // if full RTT feedback is ready, do full update

        UpdateStateUfcwnd(qp, p, ch, false);


    }else if(ack_seq <= qp->ufcc.m_lastUpdateSeq)
    {
         // do fast react


        if(qp->ufcc.state == qp->INIT)
        {
   
            if(rtt <= qp->ufcc.lastRtt)
            {
                qp->ufcc.high = false;
                
                qp->m_rate = std::min(qp->m_rate + qp->ufcc.up_rate,qp->ufcc.high_rate );

                    //printf("node:%u rtt_diff: %f up rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);

                    

                    
            }else
            {
                qp->ufcc.low = false;
                qp->m_rate = std::max(qp->m_rate - qp->ufcc.down_rate ,qp->ufcc.low_rate);   
                //printf("node:%u rtt_diff: %f down rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);

                    //("ack:%u down rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", ack_seq, qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);
            }

            //printf("node:%u rtt_diff: %f\n", m_node->GetId(),rtt_diff-1);
        }else if (qp->ufcc.state == qp->STEADY)
        {
            if(rtt <= qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.high_rate;
                //printf("node: %u low rate\n", m_node->GetId());

            }else if(rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate;
              
            }

            if(qp->ufcc.arvgRtt > qp->ufcc.minRtt + high_rtt && rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate*0.95;

            }
            if (qp->ufcc.arvgRtt < qp->ufcc.minRtt + 0.7*(low_rtt + high_rtt))
            {
                qp->m_rate = std::min(qp->m_max_rate,qp->ufcc.high_rate*1.05);

            }


            if( rtt < qp->ufcc.minRtt + 0.25*low_rtt && qp->m_rate != qp->m_max_rate)
            {
                qp->ufcc.state_count++;
            }else
            {
                qp->ufcc.state_count = 0;
            }

            if(qp->ufcc.state_count >= 1)
            {
                qp->ufcc.high_rate = qp->m_max_rate;

                qp->m_rate = qp->ufcc.high_rate;
                qp->ufcc.up_rate = 0;
                qp->ufcc.down_rate = 1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);   
                qp->ufcc.low = false;
                qp->ufcc.high = false;
                
                qp->ufcc.state = qp->INIT;
                qp->ufcc.state_count = 0;
            }

        }else if (qp->ufcc.state == qp->BURST)
        {
            qp->m_rate = std::max(0.3*qp->ufcc.low_rate , m_minRate);
   
            if( rtt <= qp->ufcc.minRtt + burst_rtt)
            {
                qp->m_rate = std::min((0.9*qp->ufcc.low_rate+ qp->ufcc.high_rate)/2, 2*0.9*qp->ufcc.low_rate);
               
            }

        }

    }

    ChangeRate(qp,qp->m_rate);
}

void
RdmaHw::UpdateStateUfcwnd(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us) const
{
    


    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;



    if (qp->ufcc.lastRtt != 0)
    { // not first RTT


        //printf("%lu node:%u rtt_diff:%f rate:%lu tar_rate:%lu Rtt:%lu\n",Simulator::Now().GetTimeStep(),m_node->GetId(), rtt_diff , qp->m_rate.GetBitRate(),qp->ufcc.m_tarRate.GetBitRate(), rtt);




        if(qp->ufcc.state == qp->INIT)
        {
            if(rtt <= qp->ufcc.lastRtt)
            {
                qp->ufcc.high = false;               
                qp->m_rate = std::min(qp->m_rate + qp->ufcc.up_rate,qp->ufcc.high_rate );
               // printf("node:%u rtt_diff: %f up rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);

                
            }else 
            {
                qp->ufcc.low = false;
                qp->m_rate = std::max(qp->m_rate - qp->ufcc.down_rate ,qp->ufcc.low_rate);   
               // printf("node:%u rtt_diff: %f down rate: %lu current rate: %lu rtt: %lu last rtt: %lu\n", m_node->GetId(), qp->ufcc.down_rate, qp->m_rate, rtt, qp->ufcc.lastRtt);
            }

            if(qp->ufcc.low )
            {
                qp->ufcc.low_rate = qp->ufcc.last_rate;
            }
            if(qp->ufcc.high)
            {
                qp->ufcc.high_rate = qp->ufcc.last_rate;
            }

            qp->ufcc.low = true;
            qp->ufcc.high = true;

            if(qp->ufcc.low_rate  >= 0.95*qp->ufcc.high_rate)
            {
                if(qp->ufcc.arvgRtt <= qp->ufcc.minRtt + 0.5*(low_rtt + high_rtt))
            {
                qp->ufcc.state_count = 0;
                qp->ufcc.state = qp->STEADY;
                }else if(qp->ufcc.arvgRtt > qp->ufcc.minRtt + high_rtt)
                {

                    qp->ufcc.state_count = qp->ufcc.state_count+3;
                }else
        {
            qp->ufcc.state_count++;
                }
                if(qp->ufcc.state_count >= 5)
            {
                    qp->m_rate = std::max(qp->ufcc.low_rate - 0.1*m_minRate,m_minRate);
                    qp->ufcc.low_rate = qp->m_rate;
                qp->ufcc.state_count = 0;
                }
                
            }


            qp->m_rate = std::max(qp->ufcc.low_rate , qp->m_rate);
            qp->m_rate = std::min(qp->ufcc.high_rate , qp->m_rate);
            
            qp->ufcc.up_rate = 1000* std::min(0.5*(qp->ufcc.high_rate - qp->m_rate),qp->ufcc.high_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);
            qp->ufcc.down_rate =1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);



        }else if (qp->ufcc.state == qp->STEADY)
        {
            if(rtt <= qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.high_rate;
                //printf("node: %u low rate\n", m_node->GetId());

            }else if(rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate;
                

            }

            if(qp->ufcc.arvgRtt > qp->ufcc.minRtt + high_rtt  && rtt > qp->ufcc.arvgRtt)
            {
                qp->m_rate = qp->ufcc.low_rate*0.95;

            }
            if (qp->ufcc.arvgRtt < qp->ufcc.minRtt + 0.7*(low_rtt + high_rtt))
            {
                qp->m_rate = std::min(qp->m_max_rate,qp->ufcc.high_rate*1.05);

            }



            if( rtt < qp->ufcc.minRtt + 0.25*low_rtt && qp->m_rate != qp->m_max_rate)
            {
                qp->ufcc.state_count++;
            }else
            {
                qp->ufcc.state_count = 0;
            }

            if(qp->ufcc.state_count >= 1)
            {
                qp->ufcc.high_rate = qp->m_max_rate;

                qp->m_rate = qp->ufcc.high_rate;
                qp->ufcc.up_rate = 0;
                qp->ufcc.down_rate = 1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);   
                qp->ufcc.low = false;
                qp->ufcc.high = false;
                qp->ufcc.state_count = 0;
                qp->ufcc.state = qp->INIT;
            }



        }else if (qp->ufcc.state == qp->RELEASE)
        {
            /* code */
        }else if (qp->ufcc.state == qp->PREEMPT)
        {



        }else if (qp->ufcc.state == qp->BURST)
        {
            if(rtt <=qp->ufcc.minRtt + burst_rtt)
            {
                if(qp->ufcc.low_rate >= 0.8*qp->ufcc.high_rate)
                {
                    qp->ufcc.high_rate =  (qp->m_max_rate+qp->ufcc.high_rate)/2;
            }else
            {
                    qp->ufcc.high_rate = (qp->ufcc.low_rate + qp->ufcc.high_rate)/2;
            }

                qp->ufcc.low_rate = std::max(0.9*qp->ufcc.low_rate, m_minRate);

                qp->m_rate = std::min((qp->ufcc.low_rate+ qp->ufcc.high_rate)/2, 2*qp->ufcc.low_rate);
                qp->ufcc.down_rate =1000*0.5* (qp->m_rate - qp->ufcc.low_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);

                qp->ufcc.up_rate = 1000* std::min(0.5*(qp->ufcc.high_rate - qp->m_rate),qp->ufcc.high_rate)/(qp->snd_nxt - qp->ufcc.m_lastUpdateSeq);
                qp->ufcc.low = false;
                qp->ufcc.high = false;
                qp->ufcc.down_rate = 0;
                    qp->ufcc.state_count = 0;
                    qp->ufcc.state = qp->INIT;

            }
        }
        
         qp->ufcc.last_rate = qp->m_rate;  
        //printf("%lu node:%u aveRTT: %lu STATE:%u rate:%lu rtt_diff:%f last_rtt: %lu rtt: %lu low_rate:%lu high_rate:%lu up_rate: %lu down_rate:%lu minRtt:%lu\n",Simulator::Now().GetTimeStep(),m_node->GetId(),qp->ufcc.arvgRtt, qp->ufcc.state, qp->m_rate.GetBitRate(),  rtt_diff , qp->ufcc.lastRtt, rtt,qp->ufcc.low_rate.GetBitRate(), qp->ufcc.high_rate.GetBitRate(),qp->ufcc.up_rate,qp->ufcc.down_rate,qp->ufcc.minRtt);
        qp->ufcc.arvgRtt = 0.3*qp->ufcc.arvgRtt + 0.7*rtt;
        qp->ufcc.m_lastUpdateSeq = qp->snd_nxt;
        qp->ufcc.lastRtt = rtt;
    }

        
        
    


    
}



/**********************
 * Patched TIMELY
 *********************/

void
RdmaHw::HandleAckPatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    if (ack_seq > qp->tmly.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        UpdateRatePatchedTimely(qp, p, ch, false);
    }
    else
    { // do fast react
        FastReactPatchedTimely(qp, p, ch);
    }
}

void
RdmaHw::UpdateRatePatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us)
{
    uint32_t next_seq = qp->snd_nxt;
    uint64_t currTime = Simulator::Now().GetNanoSeconds();
    uint64_t rtt = currTime - ch.ack.ih.ts;
    bool print = !us;
    if (qp->tmly.m_lastUpdateSeq != 0)
    { // not first RTT
        int64_t new_rtt_diff = (int64_t)rtt - (int64_t)qp->tmly.lastRtt;
        qp->tmly.lastRtt = rtt;
        double rtt_diff = (1 - m_tmly_alpha) * qp->tmly.rttDiff + m_tmly_alpha * new_rtt_diff;
        double gradient = rtt_diff / m_tmly_minRtt;
        double weight;
        DataRate new_rate;
        double error;
#if PRINT_LOG
        if (print)
            printf("%lu node:%u rtt:%lu rttDiff:%.0lf gradient:%.3lf rate:%.3lf",
                   Simulator::Now().GetTimeStep(),
                   m_node->GetId(),
                   rtt,
                   rtt_diff,
                   gradient,
                   qp->tmly.m_curRate.GetBitRate() * 1e-9);
#endif
        if (rtt < m_tmly_TLow) // newRTT < Tlow
        {
            // rate = rate + rai
            new_rate = qp->tmly.m_curRate + m_rai;
        }
        else if (rtt > m_tmly_THigh) // newRTT > Thigh
        {
            //  rate = rate * (1 - beta(1 - Thigh / new_rtt))
            new_rate = qp->tmly.m_curRate * (1 - m_tmly_beta * (1 - (double)m_tmly_THigh / rtt));
        }
        else
        {
            // weight = w(rttGradient)
            if (gradient <= -0.25)
            {
                weight = 0.0;
            }
            else if (gradient >= 0.25)
            {
                weight = 1.0;
            }
            else
            {
                weight = 2.0 * gradient + 0.5;
            }
            error = ((double)rtt - (double)m_ptmly_RttRef) / m_ptmly_RttRef;
            new_rate =
                m_rai * (1 - weight) + qp->tmly.m_curRate * (1 - m_ptmly_beta * error * weight);
        }
        qp->tmly.m_curRate = std::max(m_minRate, std::min(qp->m_max_rate, new_rate));
        ChangeRate(qp, new_rate);
        qp->tmly.rttDiff = rtt_diff;
#if PRINT_LOG
        if (print)
        {
            printf(" %c %.3lf\n", inc ? '^' : 'v', qp->m_rate.GetBitRate() * 1e-9);
        }
#endif
    }
    if (!us && next_seq > qp->tmly.m_lastUpdateSeq)
    {
        qp->tmly.m_lastUpdateSeq = next_seq;
        // update
        qp->tmly.lastRtt = rtt;
    }
    //std::cout << "[PTMLY] node:" << m_node->GetId() << " rate:" << qp->m_rate << " RTT:" << rtt
              //<< std::endl;
}

void
RdmaHw::FastReactPatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
}

/**********************
 * DCTCP
 *********************/
void
RdmaHw::HandleAckDctcp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    uint32_t ack_seq = ch.ack.seq;
    uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
    bool new_batch = false;

    // update alpha
    qp->dctcp.m_ecnCnt += (cnp > 0);
    if (ack_seq > qp->dctcp.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do alpha update
#if PRINT_LOG
        printf("%lu %s %08x %08x %u %u [%u,%u,%u] %.3lf->",
               Simulator::Now().GetTimeStep(),
               "alpha",
               qp->sip.Get(),
               qp->dip.Get(),
               qp->sport,
               qp->dport,
               qp->dctcp.m_lastUpdateSeq,
               ch.ack.seq,
               qp->snd_nxt,
               qp->dctcp.m_alpha);
#endif
        new_batch = true;
        if (qp->dctcp.m_lastUpdateSeq == 0)
        { // first RTT
            qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
            qp->dctcp.m_batchSizeOfAlpha = qp->snd_nxt / m_mtu + 1;
        }
        else
        {
            double frac = std::min(1.0, double(qp->dctcp.m_ecnCnt) / qp->dctcp.m_batchSizeOfAlpha);
            qp->dctcp.m_alpha = (1 - m_g) * qp->dctcp.m_alpha + m_g * frac;
            qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
            qp->dctcp.m_ecnCnt = 0;
            qp->dctcp.m_batchSizeOfAlpha = (qp->snd_nxt - ack_seq) / m_mtu + 1;
#if PRINT_LOG
            printf("%.3lf F:%.3lf", qp->dctcp.m_alpha, frac);
#endif
        }
#if PRINT_LOG
        printf("\n");
#endif
    }

    // check cwr exit
    if (qp->dctcp.m_caState == 1)
    {
        if (ack_seq > qp->dctcp.m_highSeq)
        {
            qp->dctcp.m_caState = 0;
        }
    }

    // check if need to reduce rate: ECN and not in CWR
    if (cnp && qp->dctcp.m_caState == 0)
    {
#if PRINT_LOG
        printf("%lu %s %08x %08x %u %u %.3lf->",
               Simulator::Now().GetTimeStep(),
               "rate",
               qp->sip.Get(),
               qp->dip.Get(),
               qp->sport,
               qp->dport,
               qp->m_rate.GetBitRate() * 1e-9);
#endif
        qp->m_rate = std::max(m_minRate, qp->m_rate * (1 - qp->dctcp.m_alpha / 2));
#if PRINT_LOG
        printf("%.3lf\n", qp->m_rate.GetBitRate() * 1e-9);
#endif
        qp->dctcp.m_caState = 1;
        qp->dctcp.m_highSeq = qp->snd_nxt;
    }

    // additive inc
    if (qp->dctcp.m_caState == 0 && new_batch)
    {
        qp->m_rate = std::min(qp->m_max_rate, qp->m_rate + m_dctcp_rai);
    }
}

/*********************
 * HPCC-PINT
 ********************/
void
RdmaHw::SetPintSmplThresh(double p)
{
    pint_smpl_thresh = (uint32_t)(65536 * p);
}

void
RdmaHw::HandleAckHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    if (rand() % 65536 >= pint_smpl_thresh)
    {
        return;
    }
    // update rate
    if (ack_seq > qp->hpccPint.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        UpdateRateHpPint(qp, p, ch, false);
    }
    else
    { // do fast react
        UpdateRateHpPint(qp, p, ch, true);
    }
}

void
RdmaHw::UpdateRateHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react)
{
    uint32_t next_seq = qp->snd_nxt;
    if (qp->hpccPint.m_lastUpdateSeq == 0)
    { // first RTT
        qp->hpccPint.m_lastUpdateSeq = next_seq;
    }
    else
    {
        // check packet INT
        IntHeader& ih = ch.ack.ih;
        double U = Pint::decode_u(ih.GetPower());

        DataRate new_rate;
        int32_t new_incStage;
        double max_c = U / m_targetUtil;

        if (max_c >= 1 || qp->hpccPint.m_incStage >= m_miThresh)
        {
            new_rate = qp->hpccPint.m_curRate / max_c + m_rai;
            new_incStage = 0;
        }
        else
        {
            new_rate = qp->hpccPint.m_curRate + m_rai;
            new_incStage = qp->hpccPint.m_incStage + 1;
        }
        if (new_rate < m_minRate)
        {
            new_rate = m_minRate;
        }
        if (new_rate > qp->m_max_rate)
        {
            new_rate = qp->m_max_rate;
        }
        ChangeRate(qp, new_rate);
        if (!fast_react)
        {
            qp->hpccPint.m_curRate = new_rate;
            qp->hpccPint.m_incStage = new_incStage;
        }
        if (!fast_react)
        {
            if (next_seq > qp->hpccPint.m_lastUpdateSeq)
            {
                qp->hpccPint.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
            }
        }
    }
}

/*********************
 * Swift
 ********************/

void
RdmaHw::HandleAckSwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    auto ih = ch.ack.ih.swift;
    // std::cout << "[SWIFT] Hops: " << ih.nhop << ", Remote Delay: " << ih.remote_delay <<
    // std::endl;
    // uint32_t ack_seq = ch.ack.seq;
    auto rtt = Simulator::Now().GetNanoSeconds() - ih.ts;
    auto fabric_delay = rtt - ih.remote_delay;

    // on receiving ack
    qp->swift.m_retransmit_cnt = 0;
    auto target_fab_delay = TargetFabDelaySwift(qp, p, ch);
    auto fab_cwnd = GetCwndSwift(qp, p, ch, target_fab_delay, fabric_delay);
    auto endpoint_cwnd = GetCwndSwift(qp, p, ch, swift_target_endpoint_delay, ih.remote_delay);
    // cap max and min cwnd, and get the lower one in fab and endpoint
    auto cwnd =
        std::max(swift_min_cwnd, std::min(swift_max_cwnd, std::min(fab_cwnd, endpoint_cwnd)));
    if (cwnd < qp->m_win)
    {
        qp->swift.m_t_last_decrease = Simulator::Now();
    }
    if (cwnd < 1)
    {
        qp->swift.m_pacing_delay = rtt * 1.0 / cwnd;
        qp->SetWin(INT_MAX); // to make sure sending is only pacing-bound, but not window-bound
        qp->swift.m_real_win = cwnd;
        qp->UpdatePacing();
    }
    else
    {
        qp->swift.m_pacing_delay = 0;
        qp->SetWin((uint32_t)cwnd);
        qp->swift.m_real_win = cwnd;
    }
    //std::cout << "[SWIFT] node: " << m_node->GetId() << ", cwnd: " << cwnd
      //        << ", delay: " << fabric_delay << std::endl;
}

// calculate target fabric delay
uint64_t
RdmaHw::TargetFabDelaySwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    double fcwnd = qp->swift.m_real_win;
    auto num_hops = ch.ack.ih.swift.nhop;
    auto alpha =
        swift_fs_range / (std::pow(swift_fs_min_cwnd, -0.5) - std::pow(swift_fs_max_cwnd, -0.5));
    auto beta = -1 * alpha / std::sqrt(swift_fs_max_cwnd);
    auto t = swift_base_target + num_hops * swift_hop_scale +
             std::max(0.0, std::min(swift_fs_range, alpha * std::pow(fcwnd, -0.5) + beta));
    return t;
}

// calculate cwnd for fabric/endpoint
double
RdmaHw::GetCwndSwift(Ptr<RdmaQueuePair> qp,
                     Ptr<Packet> p,
                     CustomHeader& ch,
                     uint64_t target_delay,
                     uint64_t curr_delay) const
{
    bool canDecrease = ch.ack.ih.swift.ts > qp->swift.m_t_last_decrease.GetNanoSeconds();
    double cwnd = qp->swift.m_real_win;
    if (curr_delay < target_delay)
    {
        if (cwnd >= 1)
        {
            // num_acked is actually the number of packets IN EVERY ACK
            // so that we can assure incrementing approx. swift_ai per RTT
            cwnd = cwnd + (double)swift_ai * (m_mtu / cwnd);
        }
        else
        {
            cwnd = cwnd + swift_ai * m_mtu;
        }
    }
    else if (canDecrease)
    {
        cwnd =
            std::max(1 - swift_beta * (curr_delay - target_delay) / curr_delay, 1 - swift_max_mdf) *
            cwnd;
    }
    return cwnd;
}

void
RdmaHw::HandleAckRttQcn(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.GetTs();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(0, 1000);
    bool ecn = false;
    if (rtt <= rtt_qcn_tmin)
    {
        ecn = false;
    }
    else if (rtt <= rtt_qcn_tmax)
    {
        auto thresh = (rtt - rtt_qcn_tmin) * 1000.0 / (rtt_qcn_tmax - rtt_qcn_tmin);
        auto rand_num = distr(gen);
        ecn = rand_num < thresh;
    }
    else
    {
        ecn = true;
    }

    // window in mtu (1000), not in bytes / seq#
    auto cwnd = qp->rttqcn.curr_win;
    if (cwnd < m_mtu)
    {
        if (ecn)
        {
            cwnd *= 1 - rtt_qcn_beta;
        }
        else
        {
            cwnd += rtt_qcn_alpha * m_mtu;
        }
    }
    else
    {
        if (ecn)
        {
            cwnd -= 0.5 * m_mtu;
        }
        else
        {
            // attempt to improve: multiply by 10
            cwnd += m_mtu * 10.0 / cwnd;
        }
    }
    //std::cout << "[RTT-QCN] node: " << m_node->GetId() << ", cwnd: " << qp->rttqcn.curr_win << "->"
              //<< cwnd << ", RTT: " << rtt << ", ecn: " << ecn << std::endl;
    qp->rttqcn.curr_win = cwnd;
    qp->m_win = (uint32_t)cwnd;
}

void
RdmaHw::HandleAckPowerQcn(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.GetTs();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(0, 1000);
    uint64_t prev_rtt = qp->powerqcn.prev_rtt == 0 ? rtt : qp->powerqcn.prev_rtt;
    if (qp->powerqcn.prev_rtt < ch.ack.ih.GetTs())
    {
        qp->powerqcn.prev_rtt = rtt;
        qp->powerqcn.last_update = Simulator::Now().GetTimeStep();
    }
    double rtt_gradient = (rtt - prev_rtt) / rtt_qcn_tmin;

    bool rtt_ecn = false;
    if (rtt <= rtt_qcn_tmin)
    {
        rtt_ecn = false;
    }
    else if (rtt <= rtt_qcn_tmax)
    {
        auto thresh = (rtt - rtt_qcn_tmin) * 1000.0 / (rtt_qcn_tmax - rtt_qcn_tmin);
        auto rand_num = distr(gen);
        rtt_ecn = rand_num < thresh;
    }
    else
    {
        rtt_ecn = true;
    }

    bool gradient_ecn = false;
    if (rtt_gradient <= powerqcn_grad_min)
    {
        gradient_ecn = false;
    }
    else if (rtt_gradient <= powerqcn_grad_max)
    {
        auto thresh =
            (rtt_gradient - powerqcn_grad_min) * 1000.0 / (powerqcn_grad_max - powerqcn_grad_min);
        auto rand_num = distr(gen);
        gradient_ecn = rand_num < thresh;
    }
    else
    {
        gradient_ecn = true;
    }

    // window in mtu (1000), not in bytes / seq#
    auto cwnd = qp->rttqcn.curr_win;
    if (cwnd < m_mtu)
    {
        if (rtt_ecn)
        {
            cwnd *= 1 - rtt_qcn_beta;
        }
        else
        {
            cwnd += rtt_qcn_alpha * m_mtu;
        }
    }
    else
    {
        if (rtt_ecn)
        {
            // cwnd -= 0.5 * m_mtu;
            if (gradient_ecn)
            {
                cwnd -= 0.7 * m_mtu;
            }
            else
            {
                cwnd -= 0.5 * m_mtu;
            }
        }
        if (!rtt_ecn)
        {
            // cwnd += m_mtu * 10.0 / cwnd;
            if (gradient_ecn)
            {
                cwnd += m_mtu * 8.0 / cwnd;
            }
            else
            {
                cwnd += m_mtu * 20.0 / cwnd;
            }
        }
    }
   //std::cout << "[RTT-QCN] node: " << m_node->GetId() << ", cwnd: " << qp->rttqcn.curr_win << "->"
              //<< cwnd << ", RTT: " << rtt << ", ecn: " << rtt_ecn << gradient_ecn << std::endl;
    qp->rttqcn.curr_win = cwnd;
    qp->m_win = (uint32_t)cwnd;
}

} // namespace ns3
