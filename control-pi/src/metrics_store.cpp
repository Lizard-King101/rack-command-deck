#include "metrics_store.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <cmath>

using json = nlohmann::json;

MetricsStore::MetricsStore(const std::string& cache_path)
    : cache_path_(cache_path) {
    if (!cache_path_.empty()) load_cache();
}

void MetricsStore::update_hello(const protocol::HelloMessage& hello) {
    std::lock_guard<std::mutex> lk(mu_);
    auto& e      = hosts_[hello.host];
    e.pdu_outlet = hello.outlet;
    e.arch       = hello.arch;
    e.os         = hello.os;
    e.scripts    = hello.scripts;
    e.services   = hello.services;
    e.online     = true;
    e.last_seen  = std::chrono::steady_clock::now();
    // Don't overwrite MAC/display_name/outlet_override — those are deck-side
    if (!cache_path_.empty()) save_cache();
}

void MetricsStore::update_metrics(const protocol::Metrics& m) {
    std::lock_guard<std::mutex> lk(mu_);
    auto& e     = hosts_[m.host];
    e.metrics   = m;
    e.online    = true;
    e.last_seen = std::chrono::steady_clock::now();
}

void MetricsStore::mark_offline(const std::string& host) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = hosts_.find(host);
    if (it != hosts_.end()) {
        it->second.online = false;
        if (!cache_path_.empty()) save_cache();
    }
}

void MetricsStore::tick_online_status(int threshold_s) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [name, e] : hosts_) {
        if (!e.online) continue;
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - e.last_seen).count();
        if (age > threshold_s) e.online = false;
    }
}

void MetricsStore::tick_history() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [name, e] : hosts_) {
        if (!e.online) continue;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - e.last_history_sample).count();
        if (elapsed < 60) continue;
        MetricSample s;
        s.cpu_pct    = e.metrics.cpu.usage_pct;
        s.cpu_temp   = e.metrics.cpu_temp_c;
        s.ram_pct    = e.metrics.memory.pct;
        s.minute_idx = minute_counter_;
        e.history.push(s);
        e.last_history_sample = now;
    }
    ++minute_counter_;
}

#ifdef EMULATOR_BUILD
void MetricsStore::seed_mock_history() {
    std::lock_guard<std::mutex> lk(mu_);
    uint16_t idx = 0;
    for (auto& [name, e] : hosts_) {
        float base_cpu  = e.metrics.cpu.usage_pct;
        float base_temp = e.metrics.cpu_temp_c;
        float base_ram  = e.metrics.memory.pct;
        for (size_t i = 0; i < HISTORY_LEN; ++i) {
            float t = (float)i / HISTORY_LEN;
            MetricSample s;
            s.cpu_pct    = base_cpu  + 15.f * std::sin(t * 6.28f * 3) + 5.f * std::sin(t * 6.28f * 11);
            s.cpu_temp   = base_temp + 8.f  * std::sin(t * 6.28f * 2) + 2.f * std::sin(t * 6.28f * 7);
            s.ram_pct    = base_ram  + 10.f * std::sin(t * 6.28f * 1.5f);
            s.cpu_pct    = std::max(2.f,   std::min(99.f, s.cpu_pct));
            s.cpu_temp   = std::max(30.f,  std::min(95.f, s.cpu_temp));
            s.ram_pct    = std::max(10.f,  std::min(95.f, s.ram_pct));
            s.minute_idx = idx++;
            e.history.push(s);
        }
    }
}
#endif

std::map<std::string, HostEntry> MetricsStore::snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    return hosts_;
}

std::optional<HostEntry> MetricsStore::get(const std::string& host) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = hosts_.find(host);
    if (it == hosts_.end()) return std::nullopt;
    return it->second;
}

// ── CRUD ──────────────────────────────────────────────────────────────────────

void MetricsStore::set_display_name(const std::string& host, const std::string& name) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = hosts_.find(host);
    if (it == hosts_.end()) return;
    it->second.display_name = name;
    if (!cache_path_.empty()) save_cache();
}

void MetricsStore::set_pdu_outlet_override(const std::string& host, int outlet) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = hosts_.find(host);
    if (it == hosts_.end()) return;
    it->second.pdu_outlet_override = outlet;
    if (!cache_path_.empty()) save_cache();
}

void MetricsStore::set_mac(const std::string& host, const std::string& mac) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = hosts_.find(host);
    if (it == hosts_.end()) return;
    it->second.mac = mac;
    if (!cache_path_.empty()) save_cache();
}

void MetricsStore::remove_host(const std::string& host) {
    std::lock_guard<std::mutex> lk(mu_);
    hosts_.erase(host);
    if (!cache_path_.empty()) save_cache();
}

// ── Persistence ───────────────────────────────────────────────────────────────

void MetricsStore::load_cache() {
    std::ifstream f(cache_path_);
    if (!f.is_open()) return;
    try {
        auto arr = json::parse(f);
        for (auto& o : arr) {
            std::string host = o.value("host", "");
            if (host.empty()) continue;
            auto& e                 = hosts_[host];
            e.mac                   = o.value("mac", "");
            e.pdu_outlet            = o.value("outlet", 0);
            e.pdu_outlet_override   = o.value("outlet_override", -1);
            e.display_name          = o.value("display_name", "");
            e.arch                  = o.value("arch", "");
            e.os                    = o.value("os", "");
            e.online                = false;
            e.metrics.host          = host;
        }
        fprintf(stderr, "[cache] loaded %zu hosts from %s\n",
                hosts_.size(), cache_path_.c_str());
    } catch (const std::exception& ex) {
        fprintf(stderr, "[cache] load error: %s\n", ex.what());
    }
}

void MetricsStore::save_cache() const {
    // Called under lock — write to a tmp file then rename for atomicity
    json arr = json::array();
    for (auto& [name, e] : hosts_) {
        arr.push_back({
            {"host",            name},
            {"mac",             e.mac},
            {"outlet",          e.pdu_outlet},
            {"outlet_override", e.pdu_outlet_override},
            {"display_name",    e.display_name},
            {"arch",            e.arch},
            {"os",              e.os},
        });
    }
    std::string tmp = cache_path_ + ".tmp";
    std::ofstream f(tmp);
    if (!f.is_open()) return;
    f << arr.dump(2);
    f.close();
    std::rename(tmp.c_str(), cache_path_.c_str());
}
