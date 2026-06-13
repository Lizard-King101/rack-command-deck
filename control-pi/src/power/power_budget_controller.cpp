#include "power_budget_controller.h"
#include <algorithm>

PowerBudgetController::PowerBudgetController(const Config& cfg, PduStore& pdu,
                                             PowerSequenceEngine& sequences)
    : cfg_(cfg.power), pdu_(pdu), sequences_(sequences) {}

bool PowerBudgetController::warning() const {
    return pdu_.healthy() && pdu_.measurements_available() &&
           pdu_.total_watts() >= cfg_.warning_watts;
}

bool PowerBudgetController::critical() const {
    return pdu_.healthy() && pdu_.measurements_available() &&
           pdu_.total_watts() >= cfg_.critical_watts;
}

void PowerBudgetController::set_enabled(bool enabled) {
    cfg_.load_shedding_enabled = enabled;
    if (!enabled) {
        shedding_ = false;
        critical_since_ = {};
    }
}

void PowerBudgetController::tick() {
    sequences_.tick();
    if (!pending_shed_group_.empty() && !sequences_.status().active) {
        if (sequences_.status().succeeded) shed_groups_.insert(pending_shed_group_);
        pending_shed_group_.clear();
    }
    if (!cfg_.load_shedding_enabled || !pdu_.healthy() || !pdu_.measurements_available()) return;
    auto now = std::chrono::steady_clock::now();
    if (!critical()) {
        critical_since_ = {};
        if (pdu_.total_watts() < cfg_.warning_watts) shedding_ = false;
        return;
    }
    if (critical_since_ == std::chrono::steady_clock::time_point{}) critical_since_ = now;
    if (std::chrono::duration_cast<std::chrono::seconds>(now - critical_since_).count()
        < cfg_.critical_hold_s) return;
    shedding_ = true;
    if (sequences_.status().active) return;
    auto groups = sequences_.groups();
    std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) {
        return a.shedding_priority > b.shedding_priority;
    });
    for (const auto& group : groups) {
        if (group.never_shed || shed_groups_.count(group.name)) continue;
        std::string error;
        if (sequences_.start_group(group.name, false, &error)) pending_shed_group_ = group.name;
        return;
    }
}
