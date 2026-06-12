#pragma once
#include "protocol.h"

namespace collector {
    protocol::CpuMetrics    cpu();
    protocol::MemoryMetrics memory();
    float                   temperature(); // degrees C, NAN if unavailable
    protocol::UptimeMetrics uptime();

    // Pass previous disk/net snapshots; returns current + computed rates
    std::vector<protocol::DiskMount> disk(const std::vector<std::string>& mounts = {});
    std::vector<protocol::NetIface>  network(const std::string& prefix = "");
} // namespace collector
