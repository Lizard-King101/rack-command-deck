#pragma once
#include "config.h"
#include <mutex>
#include <string>
#include <thread>

enum class UpdateState { Idle, Running, Succeeded, Failed };

struct UpdateStatus {
    UpdateState state = UpdateState::Idle;
    int         progress = 0;
    std::string message = "Ready to update";
};

class UpdateManager {
public:
    explicit UpdateManager(const Config::Update& config);
    ~UpdateManager();

    bool start();
    bool available() const;
    UpdateStatus status() const;

private:
    void run();
    void set_status(UpdateState state, int progress, const std::string& message);

    Config::Update config_;
    mutable std::mutex mu_;
    UpdateStatus status_;
    std::thread worker_;
};
