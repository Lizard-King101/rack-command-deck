#pragma once
#include "protocol.h"
#include <string>

namespace gpio_ctrl {
    // Requires ENABLE_GPIO cmake flag and libgpiod
    protocol::CommandResult set(const std::string& cmd_id, int pin, bool state);
} // namespace gpio_ctrl
