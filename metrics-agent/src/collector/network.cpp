#include "collector.h"
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <chrono>
#include <thread>

namespace {

struct NetSample { uint64_t rx = 0, tx = 0; };

std::map<std::string, NetSample> read_net() {
    std::ifstream f("/proc/net/dev");
    std::map<std::string, NetSample> m;
    std::string line;
    std::getline(f, line); // header 1
    std::getline(f, line); // header 2
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string iface = line.substr(0, colon);
        iface.erase(0, iface.find_first_not_of(" \t"));
        std::istringstream ss(line.substr(colon + 1));
        NetSample s;
        uint64_t v; // rx_bytes is first, tx_bytes is 9th field
        ss >> s.rx;
        for (int i = 0; i < 7; ++i) ss >> v;
        ss >> s.tx;
        m[iface] = s;
    }
    return m;
}

} // namespace

std::vector<protocol::NetIface> collector::network(const std::string& prefix) {
    auto before = read_net();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto after  = read_net();

    constexpr float dt = 0.2f;
    std::vector<protocol::NetIface> result;
    for (auto& [name, b] : after) {
        if (name == "lo") continue;
        if (!prefix.empty() && name.rfind(prefix, 0) != 0) continue;
        if (!before.count(name)) continue;
        auto& a = before.at(name);
        protocol::NetIface iface;
        iface.name   = name;
        iface.rx_bps = (b.rx - a.rx) / dt;
        iface.tx_bps = (b.tx - a.tx) / dt;
        result.push_back(iface);
    }
    return result;
}
