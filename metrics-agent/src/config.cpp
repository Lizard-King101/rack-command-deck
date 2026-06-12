#include "config.h"
#include <toml++/toml.hpp>

AgentConfig AgentConfig::load(const std::string& path) {
    auto tbl = toml::parse_file(path);
    AgentConfig c;

    c.connection.control_host = tbl["connection"]["control_host"].value_or(c.connection.control_host);
    c.connection.control_port = (uint16_t)tbl["connection"]["control_port"].value_or((int64_t)c.connection.control_port);
    c.connection.reconnect_ms = (int)tbl["connection"]["reconnect_ms"].value_or((int64_t)c.connection.reconnect_ms);

    c.metrics.interval_ms = (int)tbl["metrics"]["interval_ms"].value_or((int64_t)c.metrics.interval_ms);
    c.metrics.cpu         = tbl["metrics"]["cpu"].value_or(c.metrics.cpu);
    c.metrics.memory      = tbl["metrics"]["memory"].value_or(c.metrics.memory);
    c.metrics.disk        = tbl["metrics"]["disk"].value_or(c.metrics.disk);
    c.metrics.network     = tbl["metrics"]["network"].value_or(c.metrics.network);
    c.metrics.temperature = tbl["metrics"]["temperature"].value_or(c.metrics.temperature);
    c.metrics.uptime      = tbl["metrics"]["uptime"].value_or(c.metrics.uptime);
    c.metrics.net_prefix  = tbl["metrics"]["net_prefix"].value_or(std::string{});

    if (auto arr = tbl["metrics"]["disk_mounts"].as_array())
        for (auto& v : *arr)
            if (auto s = v.value<std::string>()) c.metrics.disk_mounts.push_back(*s);

    c.controls.allow_reboot   = tbl["controls"]["allow_reboot"].value_or(false);
    c.controls.allow_shutdown = tbl["controls"]["allow_shutdown"].value_or(false);
    c.controls.allow_gpio     = tbl["controls"]["allow_gpio"].value_or(false);

    if (auto arr = tbl["controls"]["allowed_services"].as_array())
        for (auto& v : *arr)
            if (auto s = v.value<std::string>()) c.controls.allowed_services.push_back(*s);

    c.pdu.outlet = (int)tbl["pdu"]["outlet"].value_or(0LL);

    if (auto* arr = tbl["scripts"].as_array())
        for (auto& node : *arr)
            if (auto* t = node.as_table()) {
                ScriptDef s;
                s.name    = (*t)["name"].value_or(std::string{});
                s.command = (*t)["command"].value_or(std::string{});
                if (!s.name.empty()) c.scripts.push_back(s);
            }

    if (auto* arr = tbl["services"].as_array())
        for (auto& node : *arr)
            if (auto* t = node.as_table()) {
                ServiceDef s;
                s.name = (*t)["name"].value_or(std::string{});
                if (!s.name.empty()) c.services.push_back(s);
            }

    return c;
}
