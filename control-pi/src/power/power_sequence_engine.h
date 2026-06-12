#pragma once
#include "activity_store.h"
#include "command_router.h"
#include "config.h"
#include "metrics_store.h"
#include "pdu/pdu_store.h"
#include "power_history_store.h"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

enum class SequenceStepState { Pending, Running, Succeeded, Failed };

struct PowerSequenceStep {
    std::string group;
    std::string host;
    std::string action;
    std::string detail;
    SequenceStepState state = SequenceStepState::Pending;
};

struct PowerSequenceStatus {
    bool active = false;
    bool startup = false;
    bool succeeded = false;
    std::string name;
    std::string message;
    std::vector<PowerSequenceStep> steps;
};

class PowerSequenceEngine {
public:
    PowerSequenceEngine(const Config& cfg, MetricsStore& metrics, PduStore& pdu,
                        CommandRouter& router, ActivityStore& activity,
                        PowerHistoryStore& history);

    bool valid() const { return validation_error_.empty(); }
    const std::string& validation_error() const { return validation_error_; }
    bool start_all(bool startup, std::string* error = nullptr);
    bool start_group(const std::string& group, bool startup, std::string* error = nullptr);
    void tick();
    PowerSequenceStatus status() const;
    std::vector<Config::PowerGroup> groups() const { return groups_; }

private:
    std::vector<std::string> topological_groups(const std::string& root, std::string& error) const;
    bool begin(bool startup, const std::vector<std::string>& order, const std::string& name,
               std::string* error);
    void start_current();
    void succeed_current(const std::string& detail);
    void fail_current(const std::string& detail);
    void publish_step(bool begin, bool ok = false, const std::string& output = "");

    Config::Power power_cfg_;
    std::vector<Config::PowerGroup> groups_;
    MetricsStore& metrics_;
    PduStore& pdu_;
    CommandRouter& router_;
    ActivityStore& activity_;
    PowerHistoryStore& history_;
    std::string validation_error_;

    mutable std::mutex mu_;
    PowerSequenceStatus status_;
    size_t current_ = 0;
    int phase_ = 0;
    std::chrono::steady_clock::time_point phase_started_;
    std::chrono::steady_clock::time_point ready_since_;
    std::string sequence_activity_id_;
    std::string step_activity_id_;
    uint64_t ids_ = 0;
};
