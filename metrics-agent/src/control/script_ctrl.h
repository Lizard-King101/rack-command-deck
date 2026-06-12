#pragma once
#include "protocol.h"
#include <string>

namespace script_ctrl {
    protocol::CommandResult run(const std::string& cmd_id,
                                const std::string& command);
}
