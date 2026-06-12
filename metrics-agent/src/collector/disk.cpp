#include "collector.h"
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <chrono>
#include <thread>
#include <sys/statvfs.h>

namespace {

struct DiskStat { uint64_t reads_sec = 0, writes_sec = 0; };

std::map<std::string, DiskStat> read_diskstats() {
    std::ifstream f("/proc/diskstats");
    std::map<std::string, DiskStat> m;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        int major, minor; std::string dev;
        ss >> major >> minor >> dev;
        uint64_t v[11]{};
        for (auto& x : v) ss >> x;
        // fields: reads, reads_merged, sectors_read, time_read,
        //         writes, writes_merged, sectors_write, time_write, ...
        m[dev] = { v[0], v[4] };
    }
    return m;
}

} // namespace

std::vector<protocol::DiskMount> collector::disk(const std::vector<std::string>& mounts) {
    auto before = read_diskstats();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto after  = read_diskstats();

    // Determine which mounts to report
    std::vector<std::string> targets = mounts;
    if (targets.empty()) {
        std::ifstream f("/proc/mounts");
        std::string dev, mp, fs, opts;
        int d1, d2;
        while (f >> dev >> mp >> fs >> opts >> d1 >> d2) {
            if (dev.rfind("/dev/", 0) == 0) targets.push_back(mp);
        }
    }

    std::vector<protocol::DiskMount> result;
    for (auto& mp : targets) {
        struct statvfs st{};
        if (statvfs(mp.c_str(), &st) != 0) continue;

        protocol::DiskMount d;
        d.path     = mp;
        d.total_kb = (uint64_t)st.f_blocks * st.f_frsize / 1024;
        uint64_t free_kb = (uint64_t)st.f_bfree * st.f_frsize / 1024;
        d.used_kb  = d.total_kb - free_kb;
        if (d.total_kb > 0) d.pct = 100.f * d.used_kb / d.total_kb;

        // Match mount point back to device for IOPS (best-effort)
        std::ifstream mf("/proc/mounts");
        std::string mdev, mmp, mfs, mopts; int x1, x2;
        while (mf >> mdev >> mmp >> mfs >> mopts >> x1 >> x2) {
            if (mmp != mp) continue;
            auto dev = mdev.substr(mdev.rfind('/') + 1);
            if (before.count(dev) && after.count(dev)) {
                constexpr float dt = 0.2f;
                d.read_kbs  = (after.at(dev).reads_sec  - before.at(dev).reads_sec)  * 512 / 1024.f / dt;
                d.write_kbs = (after.at(dev).writes_sec - before.at(dev).writes_sec) * 512 / 1024.f / dt;
            }
            break;
        }
        result.push_back(d);
    }
    return result;
}
