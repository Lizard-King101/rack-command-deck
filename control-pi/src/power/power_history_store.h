#pragma once
#include "config.h"
#include "protocol.h"
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct PowerPoint {
    int64_t ts = 0;
    int outlet = 0;
    float watts = 0;
};

struct PowerAnalytics {
    float current_watts = 0;
    float idle_watts = 0;
    float average_watts = 0;
    float peak_watts = 0;
    float typical_startup_watts = 0;
    double daily_kwh = 0;
    double monthly_kwh = 0;
    double monthly_cost = 0;
};

class PowerHistoryStore {
public:
    explicit PowerHistoryStore(const Config::Power& cfg);
    ~PowerHistoryStore();

    bool ready() const { return db_ != nullptr; }
    void record(const std::vector<protocol::OutletState>& outlets, int64_t ts = 0);
    void rollup_and_cleanup(int64_t now = 0);
    std::vector<PowerPoint> history(int outlet, int64_t since, size_t max_points = 240) const;
    PowerAnalytics analytics(int outlet, float current_watts = 0, int64_t now = 0) const;

private:
    bool exec(const char* sql) const;
    void* db_ = nullptr;
    Config::Power cfg_;
    mutable std::mutex mu_;
};
