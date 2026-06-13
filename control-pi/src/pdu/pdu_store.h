#pragma once
#include "protocol.h"
#include <vector>
#include <mutex>
#include <chrono>

// Thread-safe store for the latest PDU outlet snapshot.
class PduStore {
public:
    void update(protocol::PduSnapshot snapshot);
    std::vector<protocol::OutletState> get() const;
    // Returns the outlet state for a specific outlet number (1-based), or nullopt
    std::optional<protocol::OutletState> get_outlet(int outlet) const;
    void mark_poll_failed();
    bool healthy() const;
    std::optional<std::chrono::system_clock::time_point> last_successful_poll() const;
    float total_watts() const;
    float inlet_amps() const;
    float nominal_volts() const;
    bool measurements_available() const;

private:
    mutable std::mutex                   mu_;
    std::vector<protocol::OutletState>   outlets_;
    float total_watts_ = 0;
    float inlet_amps_ = 0;
    float nominal_volts_ = 0;
    bool measurements_available_ = false;
    bool healthy_ = false;
    std::optional<std::chrono::system_clock::time_point> last_successful_poll_;
};
