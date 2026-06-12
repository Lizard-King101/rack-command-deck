#pragma once
#include "protocol.h"
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

enum class ActivityState { Pending, Succeeded, Failed };

struct CommandActivity {
    std::string   id;
    std::string   host;
    std::string   action;
    std::string   detail;
    std::string   output;
    ActivityState state = ActivityState::Pending;
    int64_t       created_at = 0;
    int64_t       completed_at = 0;
};

class ActivityStore {
public:
    void begin(const protocol::CommandMessage& cmd,
               const std::string& host,
               const std::string& detail = "");
    void complete(const protocol::CommandResult& result);

    std::vector<CommandActivity> snapshot(size_t limit = 50) const;
    std::vector<CommandActivity> for_host(const std::string& host,
                                          size_t limit = 10) const;

private:
    static constexpr size_t MAX_ACTIVITIES = 100;

    mutable std::mutex          mu_;
    std::deque<CommandActivity> activities_;
};
