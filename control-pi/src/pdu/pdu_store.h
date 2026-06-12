#pragma once
#include "protocol.h"
#include <vector>
#include <mutex>
#include <chrono>

// Thread-safe store for the latest PDU outlet snapshot.
class PduStore {
public:
    void update(std::vector<protocol::OutletState> outlets);
    std::vector<protocol::OutletState> get() const;
    // Returns the outlet state for a specific outlet number (1-based), or nullopt
    std::optional<protocol::OutletState> get_outlet(int outlet) const;
    void mark_poll_failed();
    bool healthy() const;
    std::optional<std::chrono::system_clock::time_point> last_successful_poll() const;
    float total_watts() const;

private:
    mutable std::mutex                   mu_;
    std::vector<protocol::OutletState>   outlets_;
    bool healthy_ = false;
    std::optional<std::chrono::system_clock::time_point> last_successful_poll_;
};
