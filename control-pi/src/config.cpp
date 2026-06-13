#include "config.h"
#include <toml++/toml.hpp>

Config Config::load(const std::string& path) {
    auto tbl = toml::parse_file(path);
    Config c;

    c.server.port       = (uint16_t)tbl["server"]["port"].value_or((int64_t)c.server.port);
    c.server.bind       = tbl["server"]["bind"].value_or(c.server.bind);
    c.server.cache_file = tbl["server"]["cache_file"].value_or(c.server.cache_file);

    c.display.brightness    = (int)tbl["display"]["brightness"].value_or((int64_t)c.display.brightness);
    c.display.screensaver_s = (int)tbl["display"]["screensaver_s"].value_or((int64_t)c.display.screensaver_s);
    c.display.fb_device     = tbl["display"]["fb_device"].value_or(c.display.fb_device);
    c.display.touch_device  = tbl["display"]["touch_device"].value_or(c.display.touch_device);
    c.display.width         = (int)tbl["display"]["width"].value_or((int64_t)c.display.width);
    c.display.height        = (int)tbl["display"]["height"].value_or((int64_t)c.display.height);

    c.pdu.enabled  = tbl["pdu"]["enabled"].value_or(c.pdu.enabled);
    c.pdu.host     = tbl["pdu"]["host"].value_or(c.pdu.host);
    c.pdu.port     = (uint16_t)tbl["pdu"]["port"].value_or((int64_t)c.pdu.port);
    c.pdu.access_token = tbl["pdu"]["access_token"].value_or(c.pdu.access_token);
    c.pdu.nominal_voltage = (float)tbl["pdu"]["nominal_voltage"].value_or(
        (double)c.pdu.nominal_voltage);
    c.pdu.poll_ms  = (int)tbl["pdu"]["poll_ms"].value_or((int64_t)c.pdu.poll_ms);

    c.power.database_path = tbl["power"]["database_path"].value_or(c.power.database_path);
    c.power.raw_retention_hours = (int)tbl["power"]["raw_retention_hours"].value_or(
        (int64_t)c.power.raw_retention_hours);
    c.power.rollup_retention_days = (int)tbl["power"]["rollup_retention_days"].value_or(
        (int64_t)c.power.rollup_retention_days);
    c.power.currency = tbl["power"]["currency"].value_or(c.power.currency);
    c.power.cost_per_kwh = tbl["power"]["cost_per_kwh"].value_or(c.power.cost_per_kwh);
    c.power.warning_watts = (float)tbl["power"]["warning_watts"].value_or(
        (double)c.power.warning_watts);
    c.power.critical_watts = (float)tbl["power"]["critical_watts"].value_or(
        (double)c.power.critical_watts);
    c.power.critical_hold_s = (int)tbl["power"]["critical_hold_s"].value_or(
        (int64_t)c.power.critical_hold_s);
    c.power.load_shedding_enabled = tbl["power"]["load_shedding_enabled"].value_or(
        c.power.load_shedding_enabled);
    c.power.startup_readiness_s = (int)tbl["power"]["startup_readiness_s"].value_or(
        (int64_t)c.power.startup_readiness_s);

    c.update.enabled = tbl["update"]["enabled"].value_or(c.update.enabled);
    c.update.repo_path = tbl["update"]["repo_path"].value_or(c.update.repo_path);
    c.update.helper_path = tbl["update"]["helper_path"].value_or(c.update.helper_path);

    if (auto groups = tbl["power_group"].as_array()) {
        for (const auto& node : *groups) {
            auto* group = node.as_table();
            if (!group) continue;
            Config::PowerGroup g;
            g.name = (*group)["name"].value_or(std::string{});
            g.shedding_priority = (int)(*group)["shedding_priority"].value_or((int64_t)0);
            g.never_shed = (*group)["never_shed"].value_or(false);
            g.startup_timeout_s = (int)(*group)["startup_timeout_s"].value_or((int64_t)120);
            g.shutdown_timeout_s = (int)(*group)["shutdown_timeout_s"].value_or((int64_t)120);
            if (auto members = (*group)["members"].as_array())
                for (const auto& member : *members)
                    if (auto value = member.value<std::string>()) g.members.push_back(*value);
            if (auto deps = (*group)["dependencies"].as_array())
                for (const auto& dep : *deps)
                    if (auto value = dep.value<std::string>()) g.dependencies.push_back(*value);
            if (!g.name.empty()) c.power_groups.push_back(std::move(g));
        }
    }

    return c;
}
