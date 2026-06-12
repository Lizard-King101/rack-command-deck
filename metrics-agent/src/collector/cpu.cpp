#include "collector.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

namespace {

struct CpuRaw {
    uint64_t user, nice, sys, idle, iowait, irq, softirq, steal;
    uint64_t total() const { return user+nice+sys+idle+iowait+irq+softirq+steal; }
    uint64_t busy()  const { return total() - idle - iowait; }
};

std::vector<CpuRaw> read_stat() {
    std::ifstream f("/proc/stat");
    std::vector<CpuRaw> rows;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("cpu", 0) != 0) break;
        std::istringstream ss(line.substr(line.find(' ')));
        CpuRaw r{};
        ss >> r.user >> r.nice >> r.sys >> r.idle
           >> r.iowait >> r.irq >> r.softirq >> r.steal;
        rows.push_back(r);
    }
    return rows;
}

float pct(const CpuRaw& a, const CpuRaw& b) {
    auto dt = b.total() - a.total();
    if (dt == 0) return 0.f;
    return 100.f * (b.busy() - a.busy()) / dt;
}

} // namespace

protocol::CpuMetrics collector::cpu() {
    auto a = read_stat();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto b = read_stat();

    protocol::CpuMetrics m;
    if (a.empty() || b.empty()) return m;

    m.usage_pct = pct(a[0], b[0]);
    for (size_t i = 1; i < b.size() && i < a.size(); ++i)
        m.core_pct.push_back(pct(a[i], b[i]));
    return m;
}
