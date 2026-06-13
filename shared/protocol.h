#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace protocol {

// ── Metrics ──────────────────────────────────────────────────────────────────

struct CpuMetrics {
    float usage_pct = 0;
    std::vector<float> core_pct;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CpuMetrics, usage_pct, core_pct)

struct MemoryMetrics {
    uint64_t total_kb = 0;
    uint64_t used_kb  = 0;
    float    pct      = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MemoryMetrics, total_kb, used_kb, pct)

struct DiskMount {
    std::string path;
    uint64_t total_kb  = 0;
    uint64_t used_kb   = 0;
    float    pct       = 0;
    float    read_kbs  = 0;
    float    write_kbs = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DiskMount, path, total_kb, used_kb, pct, read_kbs, write_kbs)

struct NetIface {
    std::string name;
    float rx_bps = 0;
    float tx_bps = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetIface, name, rx_bps, tx_bps)

struct UptimeMetrics {
    double uptime_s = 0;
    float  load1    = 0;
    float  load5    = 0;
    float  load15   = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UptimeMetrics, uptime_s, load1, load5, load15)

struct Metrics {
    std::string            type = "metrics";
    std::string            host;
    int64_t                ts = 0;
    CpuMetrics             cpu;
    MemoryMetrics          memory;
    float                  cpu_temp_c = 0;
    std::vector<DiskMount> disks;
    std::vector<NetIface>  net;
    UptimeMetrics          uptime;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Metrics, type, host, ts, cpu, memory, cpu_temp_c, disks, net, uptime)

// ── Handshake ─────────────────────────────────────────────────────────────────

struct HelloScript {
    std::string name;
    std::string command;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HelloScript, name, command)

struct HelloService {
    std::string name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HelloService, name)

struct HelloMessage {
    std::string type   = "hello";
    std::string host;
    std::string arch;
    std::string os;
    int         outlet  = 0;
    std::vector<HelloScript>  scripts;
    std::vector<HelloService> services;
};

// Custom from_json: tolerates old agents that don't send scripts/services
inline void to_json(nlohmann::json& j, const HelloMessage& m) {
    j = {{"type", m.type}, {"host", m.host}, {"arch", m.arch}, {"os", m.os},
         {"outlet", m.outlet}, {"scripts", m.scripts}, {"services", m.services}};
}
inline void from_json(const nlohmann::json& j, HelloMessage& m) {
    j.at("type").get_to(m.type);
    j.at("host").get_to(m.host);
    if (j.contains("arch"))     j.at("arch").get_to(m.arch);
    if (j.contains("os"))       j.at("os").get_to(m.os);
    if (j.contains("outlet"))   j.at("outlet").get_to(m.outlet);
    if (j.contains("scripts"))  j.at("scripts").get_to(m.scripts);
    if (j.contains("services")) j.at("services").get_to(m.services);
}

// ── Commands (control Pi → agent) ────────────────────────────────────────────

struct CommandMessage {
    std::string type        = "cmd";
    std::string id;
    std::string action;      // "reboot" | "shutdown" | "service" | "gpio" | "outlet"
    std::string service_name;
    std::string service_op;  // "start" | "stop" | "restart"
    int         gpio_pin    = 0;
    bool        gpio_state  = false;
    int         outlet_num  = 0;
    bool        outlet_state = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CommandMessage, type, id, action,
    service_name, service_op, gpio_pin, gpio_state, outlet_num, outlet_state)

// ── Command result (agent → control Pi) ──────────────────────────────────────

struct CommandResult {
    std::string type   = "cmd_result";
    std::string id;
    bool        ok     = false;
    std::string output;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CommandResult, type, id, ok, output)

// ── PDU outlet state (internal to control Pi) ────────────────────────────────

struct OutletState {
    int         outlet  = 0;
    std::string name;
    bool        on      = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OutletState, outlet, name, on)

struct PduSnapshot {
    std::vector<OutletState> outlets;
    float inlet_amps = 0;
    float nominal_volts = 0;
    float estimated_watts = 0;
    bool  measurements_available = false;
};

} // namespace protocol
