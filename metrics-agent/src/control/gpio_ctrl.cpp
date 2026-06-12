#include "gpio_ctrl.h"

#ifdef ENABLE_GPIO
#include <gpiod.h>

protocol::CommandResult gpio_ctrl::set(const std::string& cmd_id, int pin, bool state) {
    gpiod_chip* chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) return { "cmd_result", cmd_id, false, "cannot open gpiochip0" };

    gpiod_line* line = gpiod_chip_get_line(chip, pin);
    if (!line) {
        gpiod_chip_close(chip);
        return { "cmd_result", cmd_id, false, "invalid pin " + std::to_string(pin) };
    }

    int rc = gpiod_line_request_output(line, "deck-agent", state ? 1 : 0);
    if (rc == 0) gpiod_line_set_value(line, state ? 1 : 0);

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return { "cmd_result", cmd_id, rc == 0,
             rc == 0 ? ("pin " + std::to_string(pin) + " set " + (state ? "high" : "low"))
                     : "gpiod request failed" };
}

#else

protocol::CommandResult gpio_ctrl::set(const std::string& cmd_id, int, bool) {
    return { "cmd_result", cmd_id, false, "built without ENABLE_GPIO" };
}

#endif
