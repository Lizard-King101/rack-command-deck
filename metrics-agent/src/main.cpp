#include "config.h"
#include "ws_client.h"
#include "control/handler.h"
#include "collector/collector.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <ctime>
#include <sys/utsname.h>

using json = nlohmann::json;
static std::atomic<bool> g_running = true;
static WsClient* g_ws = nullptr;

static void on_signal(int) { g_running = false; if (g_ws) g_ws->stop(); }

static std::string arch() {
    utsname u{}; uname(&u); return u.machine;
}
static std::string os_name() {
    utsname u{}; uname(&u);
    return std::string(u.sysname) + " " + u.release;
}

int main(int argc, char* argv[]) {
    std::string cfg_path = "/etc/command-deck/agent.toml";
    if (argc > 1) {
        cfg_path = argv[1];
    } else {
        std::ifstream probe("agent.toml");
        if (probe.good()) cfg_path = "agent.toml";
    }

    auto cfg = AgentConfig::load(cfg_path);
    ControlHandler handler(cfg);

    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));

    protocol::HelloMessage hello;
    hello.host   = hostname;
    hello.arch   = arch();
    hello.os     = os_name();
    hello.outlet = cfg.pdu.outlet;
    for (auto& s : cfg.scripts)  hello.scripts.push_back({s.name, s.command});
    for (auto& s : cfg.services) hello.services.push_back({s.name});

    // Send hello immediately on every (re)connect — no sleep needed
    WsClient ws(cfg.connection, handler, [&]{
        g_ws->enqueue(json(hello).dump());
    });
    g_ws = &ws;

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    std::thread ws_thread([&]{ ws.run(); });

    while (g_running) {
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(cfg.metrics.interval_ms);

        protocol::Metrics m;
        m.host = hostname;
        m.ts   = std::time(nullptr);

        if (cfg.metrics.cpu)         m.cpu        = collector::cpu();
        if (cfg.metrics.memory)      m.memory     = collector::memory();
        if (cfg.metrics.temperature) m.cpu_temp_c = collector::temperature();
        if (cfg.metrics.disk)        m.disks      = collector::disk(cfg.metrics.disk_mounts);
        if (cfg.metrics.network)     m.net        = collector::network(cfg.metrics.net_prefix);
        if (cfg.metrics.uptime)      m.uptime     = collector::uptime();

        ws.enqueue(json(m).dump());
        std::this_thread::sleep_until(deadline);
    }

    ws_thread.join();
    return 0;
}
