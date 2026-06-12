#include "activity_store.h"
#include <algorithm>
#include <ctime>

void ActivityStore::begin(const protocol::CommandMessage& cmd,
                          const std::string& host,
                          const std::string& detail) {
    std::lock_guard<std::mutex> lk(mu_);
    activities_.push_front({
        cmd.id, host, cmd.action, detail, "", ActivityState::Pending,
        std::time(nullptr), 0
    });
    while (activities_.size() > MAX_ACTIVITIES) activities_.pop_back();
}

void ActivityStore::complete(const protocol::CommandResult& result) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::find_if(activities_.begin(), activities_.end(),
        [&](const CommandActivity& a) { return a.id == result.id; });
    if (it == activities_.end()) return;
    it->state        = result.ok ? ActivityState::Succeeded : ActivityState::Failed;
    it->output       = result.output;
    it->completed_at = std::time(nullptr);
}

std::vector<CommandActivity> ActivityStore::snapshot(size_t limit) const {
    std::lock_guard<std::mutex> lk(mu_);
    size_t count = std::min(limit, activities_.size());
    return {activities_.begin(), activities_.begin() + count};
}

std::vector<CommandActivity> ActivityStore::for_host(const std::string& host,
                                                     size_t limit) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<CommandActivity> result;
    for (const auto& activity : activities_) {
        if (activity.host != host) continue;
        result.push_back(activity);
        if (result.size() >= limit) break;
    }
    return result;
}
