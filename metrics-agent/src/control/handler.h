#pragma once
#include "protocol.h"
#include "config.h"
#include <functional>
#include <map>
#include <string>

class ControlHandler {
public:
    explicit ControlHandler(const AgentConfig& cfg);

    // Returns a CommandResult. Never throws.
    protocol::CommandResult dispatch(const protocol::CommandMessage& cmd);

private:
    using ActionFn = std::function<protocol::CommandResult(const protocol::CommandMessage&)>;
    std::map<std::string, ActionFn> actions_;
    const AgentConfig&             cfg_;
};
