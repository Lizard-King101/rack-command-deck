#include "pdu_store.h"

void PduStore::update(std::vector<protocol::OutletState> outlets) {
    std::lock_guard<std::mutex> lk(mu_);
    outlets_ = std::move(outlets);
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
    float total = 0.f;
    for (const auto& outlet : outlets_) total += outlet.watts;
    return total;
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
