#pragma once
#include "protocol.h"
#include <array>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <chrono>

// ── 24h metric history ────────────────────────────────────────────────────────

struct MetricSample {
    float    cpu_pct  = 0;
    float    cpu_temp = 0;
    float    ram_pct  = 0;
    uint16_t minute_idx = 0;
};

static constexpr size_t HISTORY_LEN = 1440; // 24h × 60 min

struct RingBuffer {
    std::array<MetricSample, HISTORY_LEN> buf{};
    size_t head  = 0;
    size_t count = 0;

    void push(MetricSample s) {
        buf[head] = s;
        head = (head + 1) % HISTORY_LEN;
        if (count < HISTORY_LEN) ++count;
    }
};

// ── Power management state ────────────────────────────────────────────────────

enum class PowerState { Normal, ShutdownPending, WaitingWatts, OutletOff };

// ── Host entry ────────────────────────────────────────────────────────────────

struct HostEntry {
    protocol::Metrics  metrics;
    int                pdu_outlet          = 0;  // from agent hello
    int                pdu_outlet_override = -1; // deck override; -1 = use agent value
    std::string        arch;
    std::string        os;
    std::string        mac;
    std::string        display_name; // user-set label; shown instead of hostname when non-empty
    bool               online = false;
    std::chrono::steady_clock::time_point last_seen;

    std::vector<protocol::HelloScript>  scripts;
    std::vector<protocol::HelloService> services;

    RingBuffer   history;
    std::chrono::steady_clock::time_point last_history_sample;
    PowerState   power_state     = PowerState::Normal;
    std::chrono::steady_clock::time_point shutdown_started;

    int effective_pdu_outlet() const {
        return pdu_outlet_override >= 0 ? pdu_outlet_override : pdu_outlet;
    }
};

// ── MetricsStore ──────────────────────────────────────────────────────────────

class MetricsStore {
public:
    explicit MetricsStore(const std::string& cache_path = "");

    void update_hello(const protocol::HelloMessage& hello);
    void update_metrics(const protocol::Metrics& m);
    void mark_offline(const std::string& host);

    void tick_online_status(int threshold_s = 15);
    void tick_history();     // call every 60s from LVGL timer

    // Per-machine CRUD (all persist to cache)
    void set_display_name(const std::string& host, const std::string& name);
    void set_pdu_outlet_override(const std::string& host, int outlet);
    void set_mac(const std::string& host, const std::string& mac);
    void remove_host(const std::string& host);

    std::map<std::string, HostEntry> snapshot() const;
    std::optional<HostEntry>         get(const std::string& host) const;

#ifdef EMULATOR_BUILD
    void seed_mock_history(); // pre-fill ring buffers for chart preview
#endif

private:
    void load_cache();
    void save_cache() const;

    mutable std::mutex               mu_;
    std::map<std::string, HostEntry> hosts_;
    std::string                      cache_path_;
    uint16_t                         minute_counter_ = 0;
};
