#include "pdu_store.h"

void PduStore::update(protocol::PduSnapshot snapshot) {
    std::lock_guard<std::mutex> lk(mu_);
    outlets_ = std::move(snapshot.outlets);
    total_watts_ = snapshot.estimated_watts;
    inlet_amps_ = snapshot.inlet_amps;
    nominal_volts_ = snapshot.nominal_volts;
    measurements_available_ = snapshot.measurements_available;
    healthy_ = true;
    last_successful_poll_ = std::chrono::system_clock::now();
}

void PduStore::mark_poll_failed() {
    std::lock_guard<std::mutex> lk(mu_);
    healthy_ = false;
}

bool PduStore::healthy() const {
    std::lock_guard<std::mutex> lk(mu_);
    return healthy_;
}

std::optional<std::chrono::system_clock::time_point> PduStore::last_successful_poll() const {
    std::lock_guard<std::mutex> lk(mu_);
    return last_successful_poll_;
}

float PduStore::total_watts() const {
    std::lock_guard<std::mutex> lk(mu_);
    return total_watts_;
}

float PduStore::inlet_amps() const {
    std::lock_guard<std::mutex> lk(mu_);
    return inlet_amps_;
}

float PduStore::nominal_volts() const {
    std::lock_guard<std::mutex> lk(mu_);
    return nominal_volts_;
}

bool PduStore::measurements_available() const {
    std::lock_guard<std::mutex> lk(mu_);
    return measurements_available_;
}

std::vector<protocol::OutletState> PduStore::get() const {
    std::lock_guard<std::mutex> lk(mu_);
    return outlets_;
}

std::optional<protocol::OutletState> PduStore::get_outlet(int outlet) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& o : outlets_)
        if (o.outlet == outlet) return o;
    return std::nullopt;
}
