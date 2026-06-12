#include "handler.h"
#include "host_ctrl.h"
#include "service_ctrl.h"
#include "gpio_ctrl.h"
#include "script_ctrl.h"
#include <algorithm>
#include <stdexcept>

static protocol::CommandResult denied(const std::string& id, const std::string& reason) {
    return { "cmd_result", id, false, "denied: " + reason };
}

ControlHandler::ControlHandler(const AgentConfig& cfg) : cfg_(cfg) {
    actions_["reboot"] = [this](const protocol::CommandMessage& c) -> protocol::CommandResult {
        if (!cfg_.controls.allow_reboot) return denied(c.id, "allow_reboot is false");
        return host_ctrl::reboot(c.id);
    };
    actions_["shutdown"] = [this](const protocol::CommandMessage& c) -> protocol::CommandResult {
        if (!cfg_.controls.allow_shutdown) return denied(c.id, "allow_shutdown is false");
        return host_ctrl::shutdown(c.id);
    };
    actions_["wol"] = [this](const protocol::CommandMessage& c) -> protocol::CommandResult {
        // WOL is sent from the control Pi, not the agent — this action is a no-op on the agent side
        return { "cmd_result", c.id, false, "wol must be sent from control Pi" };
    };
    actions_["service"] = [this](const protocol::CommandMessage& c) -> protocol::CommandResult {
        auto& allowed = cfg_.controls.allowed_services;
        if (std::find(allowed.begin(), allowed.end(), c.service_name) == allowed.end())
            return denied(c.id, c.service_name + " not in allowed_services");
        return service_ctrl::manage(c.id, c.service_name, c.service_op);
    };
    actions_["script"] = [this](const protocol::CommandMessage& c) -> protocol::CommandResult {
        auto it = std::find_if(cfg_.scripts.begin(), cfg_.scripts.end(),
            [&](const ScriptDef& script) { return script.command == c.service_name; });
        if (it == cfg_.scripts.end()) return denied(c.id, "script is not configured");
        return script_ctrl::run(c.id, it->command);
    };
    actions_["gpio"] = [this](const protocol::CommandMessage& c) -> protocol::CommandResult {
        if (!cfg_.controls.allow_gpio) return denied(c.id, "allow_gpio is false");
        return gpio_ctrl::set(c.id, c.gpio_pin, c.gpio_state);
    };
}

protocol::CommandResult ControlHandler::dispatch(const protocol::CommandMessage& cmd) {
    auto it = actions_.find(cmd.action);
    if (it == actions_.end())
        return { "cmd_result", cmd.id, false, "unknown action: " + cmd.action };
    try {
        return it->second(cmd);
    } catch (const std::exception& e) {
        return { "cmd_result", cmd.id, false, std::string("exception: ") + e.what() };
    }
}
