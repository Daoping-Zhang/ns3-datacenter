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
 */

#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <ostream>
#include <queue>
#include <string>
#include <vector>
#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include "rapidcsv.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>
#include <ns3/rdma.h>
#include <ns3/sim-setting.h>
#include <ns3/switch-node.h>

#include <filesystem>
#include <iostream>
#include <map>
#include <unordered_map>

using namespace ns3;

enum class RUN_MODE
{
    FAIR,
    UFCC,
    CRUX
} runmode;

struct Interface
{
    uint32_t idx;
    bool up;
    uint64_t delay;
    uint64_t bw;

    Interface()
        : idx(0),
          up(false)
    {
    }
};

// emulate a ML training task, first communicate and then compute
struct Task
{
    // Task serial and number of iterations.
    int32_t num, iteration;
    // Communication and computation time in one iteration
    ns3::Time communication, computation;
};

struct RunState
{
    // the task number the host is running on
    std::array<std::optional<std::reference_wrapper<Task>>, 3> currTask;
    // current iteration of the task
    std::array<uint32_t, 3> iteration;
    // cumulative sent bytes in total (all nodes)
    uint64_t bytes_sent;
    // cumulative computation time
    Time computation_time;
} run_state;

struct QlenDistribution
{
    std::vector<uint32_t> cnt; // cnt[i] is the number of times that the queue len is i KB

    void add(uint32_t qlen)
    {
        uint32_t kb = qlen / 1000;
        if (cnt.size() < kb + 1)
        {
            cnt.resize(kb + 1);
        }
        cnt[kb]++;
    }
};

struct CurrInterval
{
    Time computing_time = Seconds(0);
    // transmitted bytes

} curr_interval;

// Constants and File Paths
const std::string BASE_PATH = "examples/unnamed-cc/";
const std::string LINK_RATE = "100Gbps";
const uint32_t SERVER_NUM = 6;
const int TOR_NUM = 2;

std::string TASK_PATH = BASE_PATH + "ml_traffic.csv";
std::string CONF_PATH = BASE_PATH + "config.txt";
std::string TASKSWAP_PATH = BASE_PATH + "eval1/taskswap.txt";
std::string fct_output_file = "fct.txt";
std::string pfc_output_file = "pfc.txt";
std::string qlen_mon_file;
NS_LOG_COMPONENT_DEFINE("UNNAMED_EVALUATION1");

// Configuration Parameters
bool enable_qcn = true;
uint32_t cc_mode = CC_MODE::UFCC;
bool clamp_target_rate = false;
bool l2_back_to_zero = false;
bool rate_bound = true;
bool multi_rate = true;
bool sample_feedback = false;
bool var_win = false;
bool fast_react = true;
bool enable_trace = true;

// Timing and Simulation Parameters
double pause_time = 5.0;
double simulator_stop_time = 3.01;
double alpha_resume_interval = 55.0;
double rp_timer;
double ewma_gain = 1.0 / 16.0;
double rate_decrease_interval = 4.0;
double error_rate_per_link = 0.0;
double pint_log_base = 1.05;
double pint_prob = 1.0;
double u_target = 0.95;

// Packet and Rate Parameters
uint32_t packet_payload_size = 1000;
uint32_t l2_chunk_size = 0;
uint32_t l2_ack_interval = 0;
std::string rate_ai;
std::string rate_hai;
std::string min_rate = "100Mb/s";
std::string dctcp_rate_ai = "1000Mb/s";

// Recovery and Window Parameters
uint32_t fast_recovery_times = 5;
uint32_t has_win = 1;
uint32_t global_t = 1;
uint32_t mi_thresh = 5;
uint32_t int_multi = 1;
uint32_t ack_high_prio = 0;

// Buffer and Queue Parameters
uint32_t buffer_size = 16;
uint32_t qlen_dump_interval = 100000000;
uint32_t qlen_mon_interval = 100;
uint64_t qlen_mon_start = 2000000000;
uint64_t qlen_mon_end = 2100000000;

// Link Down Parameters
uint64_t link_down_time = 0;
uint32_t link_down_A = 0;
uint32_t link_down_B = 0;

// Rate Mappings
std::unordered_map<uint64_t, uint32_t> rate2kmax;
std::unordered_map<uint64_t, uint32_t> rate2kmin;
std::unordered_map<uint64_t, double> rate2pmax;

// Runtime Variables
std::ifstream topof, flowf, tracef;
std::ofstream fout;

NodeContainer n;
NodeContainer servers;
NodeContainer tors;
NodeContainer all_nodes;

NetDeviceContainer switchToSwitchInterfaces;

std::map<uint32_t, uint32_t> switchNumToId;
std::map<uint32_t, uint32_t> switchIdToNum;
std::map<uint32_t, NetDeviceContainer> switchUp;
std::map<uint32_t, NetDeviceContainer> switchDown;
std::map<uint32_t, NetDeviceContainer> sourceNodes;

int num_conter[6] = {0};

uint64_t nic_rate;
uint64_t maxRtt, maxBdp;

std::map<Ptr<Node>, std::map<Ptr<Node>, Interface>> nbr2if;
std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<Ptr<Node>>>> nextHop;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairDelay;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairTxDelay;
std::map<uint32_t, std::map<uint32_t, uint64_t>> pairBw;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t>> pairBdp;
std::map<uint32_t, std::map<uint32_t, uint64_t>> pairRtt;

std::vector<Ipv4Address> serverAddress;
std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint16_t>> portNumder;

std::map<uint32_t, std::map<uint32_t, QlenDistribution>> queue_result;
std::vector<Task> tasks;

std::map<uint32_t, std::map<uint32_t, std::vector<Ptr<QbbNetDevice>>>> switchToSwitch;

/************************************************
 * Function declarations
 ***********************************************/

uint32_t ip_to_node_id(Ipv4Address ip);
Ipv4Address node_id_to_ip(uint32_t id);
void get_pfc(FILE* fout, Ptr<QbbNetDevice> dev, uint32_t type);
void monitor_buffer(FILE* qlen_output, NodeContainer* n);
void CalculateRoute(Ptr<Node> host);
void CalculateRoutes(NodeContainer& n);
void SetRoutingEntries();
void TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b);
uint64_t get_nic_rate(NodeContainer& n);
void PrintResults(std::map<uint32_t, NetDeviceContainer> ToR, uint32_t numToRs, double delay);
void PrintResultsFlow(std::map<uint32_t, NetDeviceContainer> Src, uint32_t numFlows, double delay);
void qp_finish(FILE* fout, Ptr<RdmaQueuePair> q);
void readTasks(std::string path);
void schedNextTask(uint32_t node_id);
void schedNextTransmit(uint32_t node_id);
void schedNextCompute(uint32_t node_id);

int
main(int argc, char* argv[])
{
    // input configuration
    CommandLine cmd;
    std::string temp = "ufcc";
    std::ifstream conf;
    cmd.AddValue("task-file", "task file path", TASK_PATH);
    cmd.AddValue("mode", "Run mode: fair, crux, ufcc", temp);
    cmd.Parse(argc, argv);
    readTasks(TASK_PATH);
    conf.open(CONF_PATH);
    std::filesystem::create_directories(BASE_PATH + "eval1");
    fout.open(TASKSWAP_PATH);

    // parse configuration
    if (temp == "fair")
    {
        runmode = RUN_MODE::FAIR;
    }
    else if (temp == "crux")
    {
        runmode = RUN_MODE::CRUX;
    }
    else if (temp == "ufcc")
    {
        runmode = RUN_MODE::UFCC;
    }
    else
    {
        std::cerr << "Invalid run mode" << std::endl;
        return 1;
    }

    switch (runmode)
    {
    case RUN_MODE::FAIR:
    case RUN_MODE::CRUX:
        cc_mode = CC_MODE::DCTCP;
        break;
    case RUN_MODE::UFCC:
        cc_mode = CC_MODE::UFCC;
        break;
    default:
        break;
    }

    switch (cc_mode)
    {
    case CC_MODE::TIMELY:
    case CC_MODE::UFCC:
    case CC_MODE::PATCHED_TIMELY:
    case CC_MODE::RTT_QCN:
    case CC_MODE::POWERQCN:
        // timely or patched timely, use ts
        IntHeader::mode = IntHeader::TS;
        break;
    case CC_MODE::POWERTCP:
        // hpcc, powertcp, use int
        IntHeader::mode = IntHeader::NORMAL;
        break;
    case CC_MODE::HPCC_PINT:
        // hpcc-pint
        IntHeader::mode = IntHeader::PINT;
        break;
    case CC_MODE::SWIFT:
        IntHeader::mode = IntHeader::SWIFT;
        break;
    default:
        // others, no extra header
        IntHeader::mode = IntHeader::NONE;
        break;
    }

    while (!conf.eof())
    {
        std::string key;
        conf >> key;
        if (key == "ENABLE_QCN")
        {
            uint32_t v;
            conf >> v;
            enable_qcn = v;
            if (enable_qcn)
            {
                std::cout << "ENABLE_QCN\t\t\t"
                          << "Yes"
                          << "\n";
            }
            else
            {
                std::cout << "ENABLE_QCN\t\t\t"
                          << "No"
                          << "\n";
            }
        }
        else if (key == "CLAMP_TARGET_RATE")
        {
            uint32_t v;
            conf >> v;
            clamp_target_rate = v;
            if (clamp_target_rate)
            {
                std::cout << "CLAMP_TARGET_RATE\t\t"
                          << "Yes"
                          << "\n";
            }
            else
            {
                std::cout << "CLAMP_TARGET_RATE\t\t"
                          << "No"
                          << "\n";
            }
        }
        else if (key == "PAUSE_TIME")
        {
            double v;
            conf >> v;
            pause_time = v;
            std::cout << "PAUSE_TIME\t\t\t" << pause_time << "\n";
        }
        else if (key == "PACKET_PAYLOAD_SIZE")
        {
            uint32_t v;
            conf >> v;
            packet_payload_size = v;
            std::cout << "PACKET_PAYLOAD_SIZE\t\t" << packet_payload_size << "\n";
        }
        else if (key == "L2_CHUNK_SIZE")
        {
            uint32_t v;
            conf >> v;
            l2_chunk_size = v;
            std::cout << "L2_CHUNK_SIZE\t\t\t" << l2_chunk_size << "\n";
        }
        else if (key == "L2_ACK_INTERVAL")
        {
            uint32_t v;
            conf >> v;
            l2_ack_interval = v;
            std::cout << "L2_ACK_INTERVAL\t\t\t" << l2_ack_interval << "\n";
        }
        else if (key == "L2_BACK_TO_ZERO")
        {
            uint32_t v;
            conf >> v;
            l2_back_to_zero = v;
            if (l2_back_to_zero)
            {
                std::cout << "L2_BACK_TO_ZERO\t\t\t"
                          << "Yes"
                          << "\n";
            }
            else
            {
                std::cout << "L2_BACK_TO_ZERO\t\t\t"
                          << "No"
                          << "\n";
            }
        }
        else if (key == "SIMULATOR_STOP_TIME")
        {
            double v;
            conf >> v;
            simulator_stop_time = v;
            std::cout << "SIMULATOR_STOP_TIME\t\t" << simulator_stop_time << "\n";
        }
        else if (key == "ALPHA_RESUME_INTERVAL")
        {
            double v;
            conf >> v;
            alpha_resume_interval = v;
            std::cout << "ALPHA_RESUME_INTERVAL\t\t" << alpha_resume_interval << "\n";
        }
        else if (key == "RP_TIMER")
        {
            double v;
            conf >> v;
            rp_timer = v;
            std::cout << "RP_TIMER\t\t\t" << rp_timer << "\n";
        }
        else if (key == "EWMA_GAIN")
        {
            double v;
            conf >> v;
            ewma_gain = v;
            std::cout << "EWMA_GAIN\t\t\t" << ewma_gain << "\n";
        }
        else if (key == "FAST_RECOVERY_TIMES")
        {
            uint32_t v;
            conf >> v;
            fast_recovery_times = v;
            std::cout << "FAST_RECOVERY_TIMES\t\t" << fast_recovery_times << "\n";
        }
        else if (key == "RATE_AI")
        {
            std::string v;
            conf >> v;
            rate_ai = v;
            std::cout << "RATE_AI\t\t\t\t" << rate_ai << "\n";
        }
        else if (key == "RATE_HAI")
        {
            std::string v;
            conf >> v;
            rate_hai = v;
            std::cout << "RATE_HAI\t\t\t" << rate_hai << "\n";
        }
        else if (key == "ERROR_RATE_PER_LINK")
        {
            double v;
            conf >> v;
            error_rate_per_link = v;
            std::cout << "ERROR_RATE_PER_LINK\t\t" << error_rate_per_link << "\n";
        }
        else if (key == "RATE_DECREASE_INTERVAL")
        {
            double v;
            conf >> v;
            rate_decrease_interval = v;
            std::cout << "RATE_DECREASE_INTERVAL\t\t" << rate_decrease_interval << "\n";
        }
        else if (key == "MIN_RATE")
        {
            conf >> min_rate;
            std::cout << "MIN_RATE\t\t" << min_rate << "\n";
        }
        else if (key == "FCT_OUTPUT_FILE")
        {
            conf >> fct_output_file;
            std::cout << "FCT_OUTPUT_FILE\t\t" << fct_output_file << '\n';
        }
        else if (key == "HAS_WIN")
        {
            conf >> has_win;
            std::cout << "HAS_WIN\t\t" << has_win << "\n";
        }
        else if (key == "GLOBAL_T")
        {
            conf >> global_t;
            std::cout << "GLOBAL_T\t\t" << global_t << '\n';
        }
        else if (key == "MI_THRESH")
        {
            conf >> mi_thresh;
            std::cout << "MI_THRESH\t\t" << mi_thresh << '\n';
        }
        else if (key == "VAR_WIN")
        {
            uint32_t v;
            conf >> v;
            var_win = v;
            std::cout << "VAR_WIN\t\t" << v << '\n';
        }
        else if (key == "FAST_REACT")
        {
            uint32_t v;
            conf >> v;
            fast_react = v;
            std::cout << "FAST_REACT\t\t" << v << '\n';
        }
        else if (key == "U_TARGET")
        {
            conf >> u_target;
            std::cout << "U_TARGET\t\t" << u_target << '\n';
        }
        else if (key == "INT_MULTI")
        {
            conf >> int_multi;
            std::cout << "INT_MULTI\t\t\t\t" << int_multi << '\n';
        }
        else if (key == "ACK_HIGH_PRIO")
        {
            conf >> ack_high_prio;
            std::cout << "ACK_HIGH_PRIO\t\t" << ack_high_prio << '\n';
        }
        else if (key == "DCTCP_RATE_AI")
        {
            conf >> dctcp_rate_ai;
            std::cout << "DCTCP_RATE_AI\t\t\t\t" << dctcp_rate_ai << "\n";
        }
        else if (key == "PFC_OUTPUT_FILE")
        {
            conf >> pfc_output_file;
            std::cout << "PFC_OUTPUT_FILE\t\t\t\t" << pfc_output_file << '\n';
        }
        else if (key == "LINK_DOWN")
        {
            conf >> link_down_time >> link_down_A >> link_down_B;
            std::cout << "LINK_DOWN\t\t\t\t" << link_down_time << ' ' << link_down_A << ' '
                      << link_down_B << '\n';
        }
        else if (key == "ENABLE_TRACE")
        {
            conf >> enable_trace;
            std::cout << "ENABLE_TRACE\t\t\t\t" << enable_trace << '\n';
        }
        else if (key == "KMAX_MAP")
        {
            int n_k;
            conf >> n_k;
            std::cout << "KMAX_MAP\t\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                uint32_t k;
                conf >> rate >> k;
                rate2kmax[rate] = k;
                std::cout << ' ' << rate << ' ' << k;
            }
            std::cout << '\n';
        }
        else if (key == "KMIN_MAP")
        {
            int n_k;
            conf >> n_k;
            std::cout << "KMIN_MAP\t\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                uint32_t k;
                conf >> rate >> k;
                rate2kmin[rate] = k;
                std::cout << ' ' << rate << ' ' << k;
            }
            std::cout << '\n';
        }
        else if (key == "PMAX_MAP")
        {
            int n_k;
            conf >> n_k;
            std::cout << "PMAX_MAP\t\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                double p;
                conf >> rate >> p;
                rate2pmax[rate] = p;
                std::cout << ' ' << rate << ' ' << p;
            }
            std::cout << '\n';
        }
        else if (key == "BUFFER_SIZE")
        {
            conf >> buffer_size;
            std::cout << "BUFFER_SIZE\t\t\t\t" << buffer_size << '\n';
        }
        else if (key == "QLEN_MON_FILE")
        {
            conf >> qlen_mon_file;
            std::cout << "QLEN_MON_FILE\t\t\t\t" << qlen_mon_file << '\n';
        }
        else if (key == "QLEN_MON_START")
        {
            conf >> qlen_mon_start;
            std::cout << "QLEN_MON_START\t\t\t\t" << qlen_mon_start << '\n';
        }
        else if (key == "QLEN_MON_END")
        {
            conf >> qlen_mon_end;
            std::cout << "QLEN_MON_END\t\t\t\t" << qlen_mon_end << '\n';
        }
        else if (key == "MULTI_RATE")
        {
            int v;
            conf >> v;
            multi_rate = v;
            std::cout << "MULTI_RATE\t\t\t\t" << multi_rate << '\n';
        }
        else if (key == "SAMPLE_FEEDBACK")
        {
            int v;
            conf >> v;
            sample_feedback = v;
            std::cout << "SAMPLE_FEEDBACK\t\t\t\t" << sample_feedback << '\n';
        }
    }

    has_win = 1;
    var_win = 1;
    Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
    Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));

    // 6 servers, 2 ToRs
    //    0     1     2               3     4     5
    //    |     |     |               |     |     |
    //    +-----+-----+               +-----+-----+
    //          |                           |
    //       +--+--+     100G, 1us      +--+--+
    //       |  6  |--------------------|  7  |
    //       +-----+                    +-----+
    // create nodes and ToRs
    for (uint32_t i = 0; i < SERVER_NUM; i++)
    {
        Ptr<Node> node = CreateObject<Node>();
        n.Add(node);
        all_nodes.Add(node);
        servers.Add(node);
    }
    for (uint32_t i = 0; i < TOR_NUM; i++)
    {
        Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
        n.Add(sw);
        tors.Add(sw);
        all_nodes.Add(sw);
        sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
        sw->SetNodeType(1); // set as ToR
    }

    InternetStackHelper internet;
    internet.Install(n);
    Ipv4GlobalRoutingHelper globalRoutingHelper;
    internet.SetRoutingHelper(globalRoutingHelper);

    // assign ip
    serverAddress.resize(SERVER_NUM);
    for (auto i = 0; i < SERVER_NUM; i++)
    {
        serverAddress[i] = node_id_to_ip(i);
    }

    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    rem->SetRandomVariable(uv);
    uv->SetStream(50);
    rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
    rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
    FILE* pfc_file [[maybe_unused]] = fopen(pfc_output_file.c_str(), "w");

    QbbHelper qbb;
    Ipv4AddressHelper ipv4;
    auto insertLink = [&](int src_no, int dst_no) {
        Ptr<Node> snode = n.Get(src_no);
        Ptr<Node> dnode = n.Get(dst_no);
        qbb.SetDeviceAttribute("DataRate", StringValue(LINK_RATE));
        qbb.SetChannelAttribute("Delay", StringValue("1us"));
        qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));

        NetDeviceContainer d = qbb.Install(snode, dnode);
        if (snode->GetNodeType() == 0)
        {
            Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(0));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src_no], Ipv4Mask(0xff000000)));
            sourceNodes[src_no].Add(DynamicCast<QbbNetDevice>(d.Get(0)));
        }
        if (dnode->GetNodeType() == 0)
        {
            Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(1));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst_no], Ipv4Mask(0xff000000)));
        }
        // link between switch and host
        if (snode->GetNodeType() == 0 && dnode->GetNodeType() != 0)
        {
            switchDown[switchIdToNum[dst_no]].Add(DynamicCast<QbbNetDevice>(d.Get(1)));
        }
        // link between switches
        if (snode->GetNodeType() != 0 && dnode->GetNodeType() != 0)
        {
            switchToSwitchInterfaces.Add(d);
            switchUp[switchIdToNum[src_no]].Add(DynamicCast<QbbNetDevice>(d.Get(0)));
            switchUp[switchIdToNum[dst_no]].Add(DynamicCast<QbbNetDevice>(d.Get(1)));
            switchToSwitch[src_no][dst_no].push_back(DynamicCast<QbbNetDevice>(d.Get(0)));
            switchToSwitch[src_no][dst_no].push_back(DynamicCast<QbbNetDevice>(d.Get(1)));
        }

        // used to create a graph of the topology
        nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
        nbr2if[snode][dnode].up = true;
        nbr2if[snode][dnode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
        nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
        nbr2if[dnode][snode].up = true;
        nbr2if[dnode][snode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

        // This is just to set up the connectivity between nodes. The IP addresses are useless
        std::stringstream ipstring;
        ipstring << "10." << src_no % 254 + 1 << "." << dst_no % 254 + 1 << ".0";
        // sprintf(ipstring, "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
        ipv4.SetBase(ipstring.str().c_str(), "255.255.255.0");
        ipv4.Assign(d);

        // setup PFC trace
        DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext(
            "QbbPfc",
            MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
        DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext(
            "QbbPfc",
            MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
    };
    insertLink(0, 6);
    insertLink(1, 6);
    insertLink(2, 6);
    insertLink(3, 7);
    insertLink(4, 7);
    insertLink(5, 7);
    insertLink(6, 7);

    nic_rate = get_nic_rate(n);
    auto setupSwitch = [&](int switch_no) {
        if (n.Get(switch_no)->GetNodeType() == 0)
        {
            NS_LOG_ERROR("Not switch");
            return;
        }
        Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(switch_no));
        double alpha = 0.125;
        sw->m_mmu->SetAlphaIngress(alpha);
        sw->m_mmu->SetAlphaEgress(UINT16_MAX);
        uint64_t totalHeadroom = 0;
        for (uint32_t j = 1; j < sw->GetNDevices(); j++)
        {
            for (uint32_t qu = 0; qu < 8; qu++)
            {
                Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
                // set ecn
                uint64_t rate = dev->GetDataRate().GetBitRate();
                NS_ASSERT_MSG(rate2kmin.find(rate) != rate2kmin.end(),
                              "must set kmin for each link speed");
                NS_ASSERT_MSG(rate2kmax.find(rate) != rate2kmax.end(),
                              "must set kmax for each link speed");
                NS_ASSERT_MSG(rate2pmax.find(rate) != rate2pmax.end(),
                              "must set pmax for each link speed");
                sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]);
                // set pfc
                uint64_t delay =
                    DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
                uint32_t headroom = rate * delay / 8 / 1000000000 * 3;

                sw->m_mmu->SetHeadroom(headroom, j, qu);
                totalHeadroom += headroom;
            }
        }
        sw->m_mmu->SetBufferPool(buffer_size * 1024 * 1024);
        sw->m_mmu->SetIngressPool(buffer_size * 1024 * 1024 - totalHeadroom);
        sw->m_mmu->SetEgressLosslessPool(buffer_size * 1024 * 1024);
        sw->m_mmu->node_id = sw->GetId();
    };
    setupSwitch(6);
    setupSwitch(7);

#if ENABLE_QP
    FILE* fct_output = fopen(fct_output_file.c_str(), "w");
    // install rdma driver to nodes
    for (auto i = 0; i < SERVER_NUM; i++)
    {
        auto rdmaHw = CreateObject<RdmaHw>();
        rdmaHw->SetAttribute("ClampTargetRate", BooleanValue(clamp_target_rate));
        rdmaHw->SetAttribute("AlphaResumInterval", DoubleValue(alpha_resume_interval));
        rdmaHw->SetAttribute("RPTimer", DoubleValue(rp_timer));
        rdmaHw->SetAttribute("FastRecoveryTimes", UintegerValue(fast_recovery_times));
        rdmaHw->SetAttribute("EwmaGain", DoubleValue(ewma_gain));
        rdmaHw->SetAttribute("RateAI", DataRateValue(DataRate(rate_ai)));
        rdmaHw->SetAttribute("RateHAI", DataRateValue(DataRate(rate_hai)));
        rdmaHw->SetAttribute("L2BackToZero", BooleanValue(l2_back_to_zero));
        rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(l2_chunk_size));
        rdmaHw->SetAttribute("L2AckInterval", UintegerValue(l2_ack_interval));
        rdmaHw->SetAttribute("CcMode", UintegerValue(cc_mode));
        rdmaHw->SetAttribute("RateDecreaseInterval", DoubleValue(rate_decrease_interval));
        rdmaHw->SetAttribute("MinRate", DataRateValue(DataRate(min_rate)));
        rdmaHw->SetAttribute("Mtu", UintegerValue(packet_payload_size));
        rdmaHw->SetAttribute("MiThresh", UintegerValue(mi_thresh));
        rdmaHw->SetAttribute("VarWin", BooleanValue(var_win));
        rdmaHw->SetAttribute("FastReact", BooleanValue(fast_react));
        rdmaHw->SetAttribute("MultiRate", BooleanValue(multi_rate));
        rdmaHw->SetAttribute("SampleFeedback", BooleanValue(sample_feedback));
        rdmaHw->SetAttribute("TargetUtil", DoubleValue(u_target));
        rdmaHw->SetAttribute("RateBound", BooleanValue(rate_bound));
        rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(dctcp_rate_ai)));
        auto rdma = CreateObject<RdmaDriver>();
        auto node = n.Get(i);
        rdma->SetNode(node);
        rdma->SetRdmaHw(rdmaHw);
        node->AggregateObject(rdma);
        rdma->Init();
        rdma->TraceConnectWithoutContext("QpComplete", MakeBoundCallback(qp_finish, fct_output));
    }
#endif

    // set ACK priority on hosts
    if (ack_high_prio)
    {
        RdmaEgressQueue::ack_q_idx = 0;
    }
    else
    {
        RdmaEgressQueue::ack_q_idx = 3;
    }

    CalculateRoutes(n);
    SetRoutingEntries();

    // get BDP and delay
    maxRtt = maxBdp = 0;
    uint64_t minRtt = 1e9;
    for (auto i = 0; i < SERVER_NUM; i++)
    {
        for (auto j = 0; j < SERVER_NUM; j++)
        {
            if (i == j)
            {
                continue;
            }
            uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
            uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
            uint64_t rtt = delay * 2 + txDelay;
            uint64_t bw = pairBw[i][j];
            uint64_t bdp = rtt * bw / 1000000000 / 8;
            pairBdp[n.Get(i)][n.Get(j)] = bdp;
            pairRtt[i][j] = rtt;
            if (bdp > maxBdp)
            {
                maxBdp = bdp;
            }
            if (rtt > maxRtt)
            {
                maxRtt = rtt;
            }
            if (rtt < minRtt)
            {
                minRtt = rtt;
            }
        }
    }

    // setup switch CC
    auto setupSwitchCC = [&](uint32_t i) {
        auto node = n.Get(i);
        if (node->GetNodeType() == 0)
        {
            return;
        }
        auto sw = DynamicCast<SwitchNode>(node);
        sw->SetAttribute("CcMode", UintegerValue(cc_mode));
        sw->SetAttribute("MaxRtt", UintegerValue(maxRtt));
        sw->SetAttribute("PowerEnabled", BooleanValue(1));
    };
    setupSwitchCC(6);
    setupSwitchCC(7);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Time interPacketInterval = Seconds(0.0000005 / 2);

    // maintain port number for each host
    for (auto i = 0; i < SERVER_NUM; i++)
    {
        for (auto j = 0; j < SERVER_NUM; j++)
        {
            portNumder[i][j] = 10000;
        }
    }

    topof.close();
    tracef.close();

    // test content

    // auto clientHelper = RdmaClientHelper(3, // priority group
    //                                      serverAddress[0],
    //                                      serverAddress[3],
    //                                      portNumder[0][3],
    //                                      23433,
    //                                      100000000,
    //                                      1,
    //                                      pairRtt[0][3],
    //                                      Simulator::GetMaximumSimulationTime());
    // auto appCon = clientHelper.Install(n.Get(0));
    // appCon.Start(Seconds(0));
    // std::cout << "test1" << std::endl;

    Simulator::Schedule(Seconds(0), schedNextTask, 0);
    Simulator::Schedule(Seconds(0.003), schedNextTask, 1);
    Simulator::Schedule(Seconds(0.006), schedNextTask, 2);
    Simulator::Schedule(Seconds(50 * maxRtt * 1e-9),
                        PrintResultsFlow,
                        sourceNodes,
                        3,
                        50 * maxRtt * 1e-9);

    Simulator::Stop(Seconds(simulator_stop_time));
    Simulator::Run();
    Simulator::Destroy();

    fout.close();
    return 0;
}

// Read ML tasks from ml_traffic.csv
void
readTasks(std::string path)
{
    rapidcsv::Document doc;
    try
    {
        doc = rapidcsv::Document(path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error reading ml_traffic.csv: " << e.what() << std::endl;
        return;
    }
    size_t rowCount = doc.GetRowCount();
    tasks.resize(rowCount);
    std::cout << "Tasks:" << std::endl;
    for (size_t i = 0; i < rowCount; i++)
    {
        tasks[i].num = doc.GetCell<int32_t>(0, i);
        tasks[i].iteration = doc.GetCell<int32_t>(1, i);
        tasks[i].communication = ns3::Seconds(doc.GetCell<double>(2, i));
        tasks[i].computation = ns3::Seconds(doc.GetCell<double>(3, i));
        std::cout << "Task " << tasks[i].num << " Iteration " << tasks[i].iteration
                  << " Communication " << tasks[i].communication.GetSeconds() << " Computation "
                  << tasks[i].computation.GetSeconds() << std::endl;
    }
}

// schedule task to some node, often called when a task is completed
void
schedNextTask(uint32_t node_id)
{
    std::string output = "";
    if (run_state.currTask[node_id].has_value())
    {
        // has previous task completed, remove it
        auto it = std::find_if(tasks.begin(), tasks.end(), [&](const Task& t) {
            return t.num == run_state.currTask[node_id].value().get().num;
        });
        output += "SWAPOUT node: " + std::to_string(node_id) + " task: " + std::to_string(it->num) +
                  " Bytes sent: " + std::to_string(run_state.bytes_sent) +
                  " Computation time: " + std::to_string(run_state.computation_time.GetSeconds()) +
                  " Time: " + std::to_string(Simulator::Now().GetSeconds()) +
                  "\n";
        if (it != tasks.end())
        {
            tasks.erase(it);
        }
        run_state.bytes_sent = 0;
        run_state.computation_time = Seconds(0);
    }
    // randomly choose a task
    if (tasks.size() <= 2) // all other tasks are occupied by other servers
    {
        run_state.currTask[node_id].reset();
        return;
    }
    while (true)
    {
        // randomly pick a task
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        uv->SetAttribute("Min", DoubleValue(0));
        uv->SetAttribute("Max", DoubleValue(tasks.size() - 1));
        uint32_t taskIndex = uv->GetInteger();

        // check if the task is already assigned to other servers
        auto it = std::find_if(run_state.currTask.begin(), run_state.currTask.end(), [&](auto& t) {
            return t.has_value() && t.value().get().num == taskIndex;
        });
        if (it == run_state.currTask.end()) // no duplicate
        {
            run_state.currTask[node_id] = tasks[taskIndex];
            break;
        }
    }
    output += "SWAPIN node: " + std::to_string(node_id) +
              " task: " + std::to_string(run_state.currTask[node_id].value().get().num) + 
              " time: " + std::to_string(Simulator::Now().GetSeconds());
    fout << output << std::endl;
    Simulator::Schedule(Seconds(0), schedNextTransmit, node_id);
}

// The first stage of a communicate-compute iteration
void
schedNextTransmit(uint32_t node_id)
{
    // maintain task information
    if (!run_state.currTask[node_id].has_value())
    { // no task
        return;
    }
    auto task = run_state.currTask[node_id]->get();
    if (run_state.iteration[node_id] == task.iteration)
    { // task completed
        Simulator::Schedule(Seconds(0), schedNextTask, node_id);
        return;
    }
    run_state.iteration[node_id]++;

    // transmit data
    // calculate flow size based on the communication time, assuming full link utilization
    uint64_t linkRateBps = std::stoull(LINK_RATE.substr(0, LINK_RATE.size() - 4)) * 1e9 / 8;
    uint32_t flowSize = task.communication.GetSeconds() * linkRateBps;
    auto clientHelper = RdmaClientHelper(
        3, // priority group
        serverAddress[node_id],
        serverAddress[node_id + 3], // destination is fixed to 0-3, 1-4, 2-5
        portNumder[node_id][node_id + 3],
        portNumder[node_id + 3][node_id],
        flowSize,
        has_win ? (global_t == 1 ? maxBdp : pairBdp[n.Get(node_id)][n.Get(node_id + 3)]) : 0,
        global_t == 1 ? maxRtt : pairRtt[node_id][node_id + 3],
        Simulator::GetMaximumSimulationTime());
    portNumder[node_id][node_id + 3]++;
    portNumder[node_id + 3][node_id]++;
    auto appCon = clientHelper.Install(n.Get(node_id));
    appCon.Start(Seconds(0)); // i.e. now
}

// The second stage of a communicate-compute iteration
void
schedNextCompute(uint32_t node_id)
{
    if (!run_state.currTask[node_id].has_value())
    { // no task
        return;
    }
    auto task = run_state.currTask[node_id]->get();
    Simulator::Schedule(task.computation, schedNextTransmit, node_id);
}

void
qp_finish(FILE* fout, Ptr<RdmaQueuePair> q)
{
    uint32_t sid = ip_to_node_id(q->sip);
    uint32_t did = ip_to_node_id(q->dip);
    uint64_t base_rtt = pairRtt[sid][did];
    uint64_t b = pairBw[sid][did];
    uint32_t total_bytes =
        q->m_size + ((q->m_size - 1) / packet_payload_size + 1) *
                        (CustomHeader::GetStaticWholeHeaderSize() -
                         IntHeader::GetStaticSize()); // translate to the minimum bytes required
                                                      // (with header but no INT)
    uint64_t standalone_fct = base_rtt + total_bytes * 8000000000LU / b;
    // sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
    fprintf(fout,
            "%08x %08x %u %u %lu %lu %lu %lu\n",
            q->sip.Get(),
            q->dip.Get(),
            q->sport,
            q->dport,
            q->m_size,
            q->startTime.GetTimeStep(),
            (Simulator::Now() - q->startTime).GetTimeStep(),
            standalone_fct);
    fflush(fout);

    std::cout << "Transmission finished" << std::endl;

    // remove rxQp from the receiver
    Ptr<Node> dstNode = n.Get(did);
    Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
    rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);

    if (run_state.currTask[sid].has_value())
    { // seems to be running a task
        schedNextCompute(sid);
    }
}

void
monitor_buffer(FILE* qlen_output, NodeContainer* n)
{
    for (uint32_t i = 0; i < n->GetN(); i++)
    {
        if (n->Get(i)->GetNodeType() == 1)
        { // is switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
            if (queue_result.find(i) == queue_result.end())
            {
                queue_result[i];
            }
            for (uint32_t j = 1; j < sw->GetNDevices(); j++)
            {
                uint32_t size = 0;
                for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
                {
                    {
                        size += sw->m_mmu->egress_bytes[j][k];
                    }
                }
                queue_result[i][j].add(size);
            }
        }
    }
    if (Simulator::Now().GetTimeStep() % qlen_dump_interval == 0)
    {
        fprintf(qlen_output, "time: %lu\n", Simulator::Now().GetTimeStep());
        for (auto& it0 : queue_result)
        {
            for (auto& it1 : it0.second)
            {
                fprintf(qlen_output, "%u %u", it0.first, it1.first);
                auto& dist = it1.second.cnt;
                for (uint32_t i = 0; i < dist.size(); i++)
                {
                    fprintf(qlen_output, " %u", dist[i]);
                }
            }
            fprintf(qlen_output, "\n");
        }
        fflush(qlen_output);
    }
    if (Simulator::Now().GetTimeStep() < qlen_mon_end)
    {
        Simulator::Schedule(NanoSeconds(qlen_mon_interval), &monitor_buffer, qlen_output, n);
    }
}

void
CalculateRoute(Ptr<Node> host)
{
    // queue for the BFS.
    std::vector<Ptr<Node>> q;
    // Distance from the host to each node.
    std::map<Ptr<Node>, int> dis;
    std::map<Ptr<Node>, uint64_t> delay;
    std::map<Ptr<Node>, uint64_t> txDelay;
    std::map<Ptr<Node>, uint64_t> bw;
    // init BFS.
    q.push_back(host);
    dis[host] = 0;
    delay[host] = 0;
    txDelay[host] = 0;
    bw[host] = 0xffffffffffffffffLU;
    // BFS.
    for (int i = 0; i < (int)q.size(); i++)
    {
        Ptr<Node> now = q[i];
        int d = dis[now];
        for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++)
        {
            // skip down link
            if (!it->second.up)
            {
                continue;
            }
            Ptr<Node> next = it->first;
            // If 'next' have not been visited.
            if (dis.find(next) == dis.end())
            {
                dis[next] = d + 1;
                delay[next] = delay[now] + it->second.delay;
                txDelay[next] =
                    txDelay[now] + packet_payload_size * 1000000000LU * 8 / it->second.bw;
                bw[next] = std::min(bw[now], it->second.bw);
                // we only enqueue switch, because we do not want packets to go through host as
                // middle point
                if (next->GetNodeType())
                {
                    q.push_back(next);
                }
            }
            // if 'now' is on the shortest path from 'next' to 'host'.
            if (d + 1 == dis[next])
            {
                nextHop[next][host].push_back(now);
            }
        }
    }
    for (const auto& it : delay)
    {
        pairDelay[it.first][host] = it.second;
    }
    for (const auto& it : txDelay)
    {
        pairTxDelay[it.first][host] = it.second;
    }
    for (const auto& it : bw)
    {
        pairBw[it.first->GetId()][host->GetId()] = it.second;
    }
}

void
CalculateRoutes(NodeContainer& n)
{
    for (int i = 0; i < (int)n.GetN(); i++)
    {
        Ptr<Node> node = n.Get(i);
        if (node->GetNodeType() == 0)
        {
            CalculateRoute(node);
        }
    }
}

void
SetRoutingEntries()
{
    // For each node.
    for (auto i = nextHop.begin(); i != nextHop.end(); i++)
    {
        Ptr<Node> node = i->first;
        auto& table = i->second;
        for (auto j = table.begin(); j != table.end(); j++)
        {
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            // The next hops towards the dst.
            std::vector<Ptr<Node>> nexts = j->second;
            for (int k = 0; k < (int)nexts.size(); k++)
            {
                Ptr<Node> next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx;
                if (node->GetNodeType())
                {
                    DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
                }
                else
                {
                    node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface);
                }
            }
        }
    }
}

// take down the link between a and b, and redo the routing
void
TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b)
{
    if (!nbr2if[a][b].up)
    {
        return;
    }
    // take down link between a and b
    nbr2if[a][b].up = nbr2if[b][a].up = false;
    nextHop.clear();
    CalculateRoutes(n);
    // clear routing tables
    for (uint32_t i = 0; i < n.GetN(); i++)
    {
        if (n.Get(i)->GetNodeType() == 1)
        {
            DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
        }
        else
        {
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->ClearTable();
        }
    }
    DynamicCast<QbbNetDevice>(a->GetDevice(nbr2if[a][b].idx))->TakeDown();
    DynamicCast<QbbNetDevice>(b->GetDevice(nbr2if[b][a].idx))->TakeDown();
    // reset routing table
    SetRoutingEntries();

    // redistribute qp on each host
    for (uint32_t i = 0; i < n.GetN(); i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        {
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();
        }
    }
}

uint64_t
get_nic_rate(NodeContainer& n)
{
    for (uint32_t i = 0; i < n.GetN(); i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        {
            return DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
        }
    }
    NS_LOG_WARN("Get NIC Rate: no host found");
    return -1;
}

// Per-ToR throughput and buffer occupancy
void
PrintResults(std::map<uint32_t, NetDeviceContainer> ToR, uint32_t numToRs, double delay)
{
    for (uint32_t i = 0; i < numToRs; i++)
    {
        double throughputTotal = 0;
        uint64_t torBuffer = 0;
        double power;
        for (uint32_t j = 0; j < ToR[i].GetN(); j++)
        {
            Ptr<QbbNetDevice> nd = DynamicCast<QbbNetDevice>(ToR[i].Get(j));
            //			uint64_t txBytes = nd->getTxBytes();
            uint64_t txBytes = nd->GetQueue()->getTxBytes();
            double rxBytes = nd->getNumRxBytes();

            uint64_t qlen = nd->GetQueue()->GetNBytesTotal();
            uint64_t bw = nd->GetDataRate().GetBitRate(); // maxRtt

            torBuffer += qlen;
            double throughput = double(txBytes * 8) / delay;
            if (j == 16)
            { //  ToDo. very ugly hardcode here specific to the burst evaluation scenario where 16
              //  is the receiver in flow-burstExp.txt.
                throughputTotal += throughput;
                power = (rxBytes * 8.0 / delay) * (qlen + bw * maxRtt * 1e-9) /
                        (bw * (bw * maxRtt * 1e-9));
            }
            std::cout << "ToR " << i << " Port " << j << " throughput " << throughput << " txBytes "
                      << txBytes << " qlen " << qlen << " time " << Simulator::Now().GetSeconds()
                      << " normpower " << power << std::endl;
        }
        std::cout << "ToR " << i << " Total " << 0 << " throughput " << throughputTotal
                  << " buffer " << torBuffer << " time " << Simulator::Now().GetSeconds()
                  << std::endl;
    }
    Simulator::Schedule(Seconds(delay), PrintResults, ToR, numToRs, delay);
}

// Per-flow (sender) information
void
PrintResultsFlow(std::map<uint32_t, NetDeviceContainer> Src, uint32_t numFlows, double delay)
{
    for (uint32_t i = 0; i < numFlows; i++) // for each src (device)
    {
        double throughputTotal = 0;
        uint64_t txBytes = 0;

        for (uint32_t j = 0; j < Src[i].GetN(); j++)
        {
            Ptr<QbbNetDevice> nd = DynamicCast<QbbNetDevice>(Src[i].Get(j));
            txBytes += nd->getNumTxBytes();
            run_state.bytes_sent += txBytes;
            uint64_t _qlen [[maybe_unused]] = nd->GetQueue()->GetNBytesTotal();
            double throughput = double(txBytes * 8) / delay;
            throughputTotal += throughput;
            // std::cout << "Src " << i << " Port " << j << " throughput "<< throughput << " txBytes
            // " << txBytes << " qlen " << qlen << " time " << Simulator::Now().GetSeconds() <<
            // std::endl;
        }
        std::cout << "Src " << i << " Total " << 0 << " throughput " << throughputTotal << " time "
                  << Simulator::Now().GetSeconds() << std::endl;

        if (txBytes == 0) // computing, not transmitting
        {
            run_state.computation_time += Seconds(delay);
        }
        else
        {
            run_state.bytes_sent += txBytes;
        }
    }
    Simulator::Schedule(Seconds(delay), PrintResultsFlow, Src, numFlows, delay);
}

void
get_pfc(FILE* fout, Ptr<QbbNetDevice> dev, uint32_t type)
{
    fprintf(fout,
            "%lu %u %u %u %u\n",
            Simulator::Now().GetTimeStep(),
            dev->GetNode()->GetId(),
            dev->GetNode()->GetNodeType(),
            dev->GetIfIndex(),
            type);
}

Ipv4Address
node_id_to_ip(uint32_t id)
{
    return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

uint32_t
ip_to_node_id(Ipv4Address ip)
{
    return (ip.Get() >> 8) & 0xffff;
}
