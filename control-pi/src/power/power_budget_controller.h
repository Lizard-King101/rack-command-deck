#pragma once
#include "config.h"
#include "power_sequence_engine.h"
#include "pdu/pdu_store.h"
#include <chrono>
#include <set>
#include <string>

class PowerBudgetController {
public:
    PowerBudgetController(const Config& cfg, PduStore& pdu, PowerSequenceEngine& sequences);
    void tick();
    bool warning() const;
    bool critical() const;
    bool shedding() const { return shedding_; }
    bool enabled() const { return cfg_.load_shedding_enabled; }
    void set_enabled(bool enabled);
    const std::set<std::string>& shed_groups() const { return shed_groups_; }

private:
    Config::Power cfg_;
    PduStore& pdu_;
    PowerSequenceEngine& sequences_;
    std::chrono::steady_clock::time_point critical_since_;
    bool shedding_ = false;
    std::set<std::string> shed_groups_;
    std::string pending_shed_group_;
};
