#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct ScriptDef  { std::string name, command; };
struct ServiceDef { std::string name; };

struct AgentConfig {
    struct Connection {
        std::string control_host = "127.0.0.1";
        uint16_t    control_port = 8765;
        int         reconnect_ms = 5000;
    } connection;

    struct Metrics {
        int                      interval_ms = 2000;
        bool                     cpu         = true;
        bool                     memory      = true;
        bool                     disk        = true;
        bool                     network     = true;
        bool                     temperature = true;
        bool                     uptime      = true;
        std::string              net_prefix;
        std::vector<std::string> disk_mounts;
    } metrics;

    struct Controls {
        bool                     allow_reboot   = false;
        bool                     allow_shutdown  = false;
        bool                     allow_gpio      = false;
        std::vector<std::string> allowed_services;
    } controls;

    struct Pdu {
        int outlet = 0; // 1–8; 0 = unassigned
    } pdu;

    std::vector<ScriptDef>  scripts;
    std::vector<ServiceDef> services;

    static AgentConfig load(const std::string& path);
};
