#pragma once
#include <string>
#include <cstdint>
#include <vector>

struct Config {
    struct Server {
        uint16_t    port       = 8765;
        std::string bind       = "0.0.0.0";
        std::string cache_file = "hosts.json";
    } server;

    struct Display {
        int         brightness    = 80;
        int         screensaver_s = 0;
        std::string fb_device     = "/dev/fb0";
        std::string touch_device  = "/dev/input/event0";
        int         width         = 800;
        int         height        = 480;
    } display;

    struct Pdu {
        bool        enabled  = false;
        std::string host;
        uint16_t    port     = 80;
        std::string access_token;
        float       nominal_voltage = 120.f;
        int         poll_ms  = 5000;
    } pdu;

    struct Power {
        std::string database_path = "power.db";
        int         raw_retention_hours = 24;
        int         rollup_retention_days = 30;
        std::string currency = "USD";
        double      cost_per_kwh = 0.15;
        float       warning_watts = 600.f;
        float       critical_watts = 750.f;
        int         critical_hold_s = 30;
        bool        load_shedding_enabled = false;
        int         startup_readiness_s = 10;
    } power;

    struct Update {
        bool        enabled = false;
        std::string release_url;
        std::string helper_path = "/usr/local/libexec/command-deck-update";
    } update;

    struct PowerGroup {
        std::string name;
        std::vector<std::string> members;
        std::vector<std::string> dependencies;
        int  shedding_priority = 0;
        bool never_shed = false;
        int  startup_timeout_s = 120;
        int  shutdown_timeout_s = 120;
    };
    std::vector<PowerGroup> power_groups;

    static Config load(const std::string& path);
};
