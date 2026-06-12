#include "collector.h"
#include <fstream>

protocol::UptimeMetrics collector::uptime() {
    protocol::UptimeMetrics m;
    {
        std::ifstream f("/proc/uptime");
        double idle;
        f >> m.uptime_s >> idle;
    }
    {
        std::ifstream f("/proc/loadavg");
        f >> m.load1 >> m.load5 >> m.load15;
    }
    return m;
}
