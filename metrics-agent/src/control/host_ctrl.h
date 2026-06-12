#pragma once
#include "protocol.h"
#include <string>

namespace host_ctrl {
    protocol::CommandResult reboot(const std::string& cmd_id);
    protocol::CommandResult shutdown(const std::string& cmd_id);
    // Send WOL magic packet to target MAC — called from control Pi, not agent
    bool send_wol(const std::string& mac_address, const std::string& broadcast = "255.255.255.255");
} // namespace host_ctrl
