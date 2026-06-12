#include "collector.h"
#include <fstream>
#include <string>

protocol::MemoryMetrics collector::memory() {
    std::ifstream f("/proc/meminfo");
    protocol::MemoryMetrics m;
    std::string key;
    uint64_t val;
    std::string unit;
    uint64_t available = 0;

    while (f >> key >> val) {
        f >> unit; // "kB"
        if (key == "MemTotal:")     m.total_kb  = val;
        else if (key == "MemAvailable:") available = val;
        if (m.total_kb && available) break;
    }
    m.used_kb = m.total_kb - available;
    if (m.total_kb > 0)
        m.pct = 100.f * m.used_kb / m.total_kb;
    return m;
}
