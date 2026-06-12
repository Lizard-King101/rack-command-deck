#pragma once
#include "protocol.h"
#include <string>

namespace service_ctrl {
    // op: "start" | "stop" | "restart" | "status"
    protocol::CommandResult manage(const std::string& cmd_id,
                                   const std::string& service,
                                   const std::string& op);
} // namespace service_ctrl
