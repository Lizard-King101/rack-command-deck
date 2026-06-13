#include "power_sequence_engine.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>

PowerSequenceEngine::PowerSequenceEngine(const Config& cfg, MetricsStore& metrics, PduStore& pdu,
                                         CommandRouter& router, ActivityStore& activity)
    : power_cfg_(cfg.power), groups_(cfg.power_groups), metrics_(metrics), pdu_(pdu),
      router_(router), activity_(activity) {
    std::set<std::string> names;
    for (const auto& group : groups_) {
        if (group.name.empty() || !names.insert(group.name).second) {
            validation_error_ = "power group names must be unique and non-empty";
            return;
        }
    }
    std::string error;
    topological_groups("", error);
    validation_error_ = error;
}

std::vector<std::string> PowerSequenceEngine::topological_groups(const std::string& root,
                                                                  std::string& error) const {
    std::map<std::string, const Config::PowerGroup*> by_name;
    for (const auto& group : groups_) by_name[group.name] = &group;
    if (!root.empty() && !by_name.count(root)) {
        error = "unknown power group: " + root;
        return {};
    }
    std::map<std::string, int> marks;
    std::vector<std::string> order;
    std::function<bool(const std::string&)> visit = [&](const std::string& name) {
        if (!by_name.count(name)) {
            error = "missing power group dependency: " + name;
            return false;
        }
        if (marks[name] == 1) {
            error = "cycle in power group dependencies at: " + name;
            return false;
        }
        if (marks[name] == 2) return true;
        marks[name] = 1;
        for (const auto& dep : by_name[name]->dependencies)
            if (!visit(dep)) return false;
        marks[name] = 2;
        order.push_back(name);
        return true;
    };
    if (!root.empty()) visit(root);
    else for (const auto& group : groups_) if (!visit(group.name)) break;
    return error.empty() ? order : std::vector<std::string>{};
}

bool PowerSequenceEngine::start_all(bool startup, std::string* error) {
    std::string local;
    auto order = topological_groups("", local);
    if (!local.empty()) {
        if (error) *error = local;
        return false;
    }
    return begin(startup, order, startup ? "rack startup" : "rack shutdown",
                 error ? error : &local);
}

bool PowerSequenceEngine::start_group(const std::string& group, bool startup, std::string* error) {
    std::string local;
    auto order = topological_groups(group, local);
    if (!local.empty()) {
        if (error) *error = local;
        return false;
    }
    if (!startup) order = {group};
    return begin(startup, order, group + (startup ? " startup" : " shutdown"),
                 error ? error : &local);
}

bool PowerSequenceEngine::begin(bool startup, const std::vector<std::string>& order,
                                const std::string& name, std::string* error) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!validation_error_.empty()) {
        if (error) *error = validation_error_;
        return false;
    }
    if (status_.active) {
        if (error) *error = "another power sequence is active";
        return false;
    }
    if (order.empty()) {
        if (error) *error = "sequence has no groups";
        return false;
    }
    status_ = {};
    status_.active = true;
    status_.startup = startup;
    status_.name = name;
    std::vector<std::string> group_order = order;
    if (!startup) std::reverse(group_order.begin(), group_order.end());
    for (const auto& group_name : group_order) {
        auto it = std::find_if(groups_.begin(), groups_.end(),
            [&](const auto& group) { return group.name == group_name; });
        if (it == groups_.end()) continue;
        auto members = it->members;
        if (!startup) std::reverse(members.begin(), members.end());
        for (const auto& host : members)
            status_.steps.push_back({group_name, host, startup ? "startup" : "shutdown",
                                     "", SequenceStepState::Pending});
    }
    if (status_.steps.empty()) {
        status_.active = false;
        if (error) *error = "sequence has no hosts";
        return false;
    }
    current_ = 0;
    phase_ = 0;
    sequence_activity_id_ = "power_sequence_" + std::to_string(++ids_);
    protocol::CommandMessage cmd;
    cmd.id = sequence_activity_id_;
    cmd.action = startup ? "sequence_startup" : "sequence_shutdown";
    activity_.begin(cmd, "", name);
    return true;
}

void PowerSequenceEngine::publish_step(bool begin, bool ok, const std::string& output) {
    if (begin) {
        step_activity_id_ = "power_step_" + std::to_string(++ids_);
        protocol::CommandMessage cmd;
        cmd.id = step_activity_id_;
        cmd.action = status_.steps[current_].action;
        activity_.begin(cmd, status_.steps[current_].host, status_.steps[current_].group);
    } else {
        activity_.complete({"cmd_result", step_activity_id_, ok, output});
    }
}

void PowerSequenceEngine::start_current() {
    auto& step = status_.steps[current_];
    step.state = SequenceStepState::Running;
    phase_started_ = std::chrono::steady_clock::now();
    ready_since_ = {};
    phase_ = 0;
    publish_step(true);
}

void PowerSequenceEngine::succeed_current(const std::string& detail) {
    auto& step = status_.steps[current_];
    step.state = SequenceStepState::Succeeded;
    step.detail = detail;
    publish_step(false, true, detail);
    ++current_;
    phase_ = 0;
    if (current_ >= status_.steps.size()) {
        status_.active = false;
        status_.succeeded = true;
        status_.message = "sequence complete";
        activity_.complete({"cmd_result", sequence_activity_id_, true, status_.message});
    }
}

void PowerSequenceEngine::fail_current(const std::string& detail) {
    auto& step = status_.steps[current_];
    step.state = SequenceStepState::Failed;
    step.detail = detail;
    status_.active = false;
    status_.message = detail;
    publish_step(false, false, detail);
    activity_.complete({"cmd_result", sequence_activity_id_, false, detail});
}

void PowerSequenceEngine::tick() {
    std::lock_guard<std::mutex> lk(mu_);
    if (!status_.active || current_ >= status_.steps.size()) return;
    if (status_.steps[current_].state == SequenceStepState::Pending) start_current();
    auto& step = status_.steps[current_];
    auto host = metrics_.get(step.host);
    if (!host) return fail_current("unknown host: " + step.host);
    auto group = std::find_if(groups_.begin(), groups_.end(),
        [&](const auto& g) { return g.name == step.group; });
    if (group == groups_.end()) return fail_current("unknown group: " + step.group);
    const int timeout = status_.startup ? group->startup_timeout_s : group->shutdown_timeout_s;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - phase_started_).count();
    if (elapsed > timeout) return fail_current(step.action + " timeout for " + step.host);
    int outlet_num = host->effective_pdu_outlet();
    auto outlet = pdu_.get_outlet(outlet_num);

    if (status_.startup) {
        if (host->online) {
            if (ready_since_ == std::chrono::steady_clock::time_point{}) ready_since_ = now;
            auto ready = std::chrono::duration_cast<std::chrono::seconds>(now - ready_since_).count();
            if (ready >= power_cfg_.startup_readiness_s) succeed_current("host ready");
            return;
        }
        ready_since_ = {};
        if (phase_ == 0) {
            if (outlet_num > 0 && !pdu_.healthy()) return fail_current("PDU disconnected");
            if (outlet_num > 0 && !outlet) return fail_current("outlet state unavailable");
            if (outlet && !outlet->on) {
                if (pdu_.measurements_available() &&
                    pdu_.total_watts() >= power_cfg_.critical_watts)
                    return fail_current("blocked by critical rack budget");
                protocol::CommandMessage cmd;
                cmd.action = "outlet";
                cmd.outlet_state = true;
                router_.dispatch(cmd, step.host, outlet_num);
            }
            if (!host->mac.empty()) router_.dispatch_wol(step.host, host->mac);
            phase_ = 1;
        }
        return;
    }

    if (phase_ == 0) {
        if (!host->online) phase_ = 1;
        else {
            protocol::CommandMessage cmd;
            cmd.action = "shutdown";
            router_.dispatch(cmd, step.host, outlet_num);
            phase_ = 1;
        }
    }
    if (phase_ == 1 && !host->online) phase_ = 2;
    if (phase_ == 2) {
        if (outlet_num <= 0) return succeed_current("host offline");
        if (!pdu_.healthy()) return fail_current("PDU disconnected");
        outlet = pdu_.get_outlet(outlet_num);
        if (!outlet) return fail_current("outlet state unavailable");
        if (outlet->on) {
            protocol::CommandMessage cmd;
            cmd.action = "outlet";
            cmd.outlet_state = false;
            router_.dispatch(cmd, step.host, outlet_num);
            phase_ = 3;
            return;
        }
        succeed_current("host offline and outlet disabled");
    }
    if (phase_ == 3) {
        if (!pdu_.healthy()) return fail_current("PDU disconnected");
        outlet = pdu_.get_outlet(outlet_num);
        if (!outlet) return fail_current("outlet state unavailable");
        if (!outlet->on) succeed_current("host offline and outlet disabled");
    }
}

PowerSequenceStatus PowerSequenceEngine::status() const {
    std::lock_guard<std::mutex> lk(mu_);
    return status_;
}
