#include "update_manager.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

UpdateManager::UpdateManager(const Config::Update& config) : config_(config) {}

UpdateManager::~UpdateManager() {
    if (worker_.joinable()) worker_.join();
}

bool UpdateManager::available() const {
#ifdef EMULATOR_BUILD
    return false;
#else
    return config_.enabled && !config_.repo_path.empty() && !config_.helper_path.empty();
#endif
}

UpdateStatus UpdateManager::status() const {
    std::lock_guard<std::mutex> lock(mu_);
    return status_;
}

bool UpdateManager::start() {
    if (!available()) {
        set_status(UpdateState::Failed, 0, "Updater is not configured");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (status_.state == UpdateState::Running) return false;
    }
    if (worker_.joinable()) worker_.join();
    set_status(UpdateState::Running, 0, "Starting update");
    worker_ = std::thread([this] { run(); });
    return true;
}

void UpdateManager::set_status(UpdateState state, int progress, const std::string& message) {
    std::lock_guard<std::mutex> lock(mu_);
    status_.state = state;
    status_.progress = std::clamp(progress, 0, 100);
    status_.message = message;
}

void UpdateManager::run() {
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        set_status(UpdateState::Failed, 0, "Could not create updater output pipe");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[1]);
        execl("/usr/bin/sudo", "sudo", "-n", config_.helper_path.c_str(),
              config_.repo_path.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(pipe_fds[1]);
    if (pid < 0) {
        close(pipe_fds[0]);
        set_status(UpdateState::Failed, 0, "Could not start updater");
        return;
    }

    FILE* output = fdopen(pipe_fds[0], "r");
    char line[512];
    std::string error;
    std::string last_output;
    while (output && fgets(line, sizeof(line), output)) {
        std::string text(line);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        if (text.rfind("PROGRESS ", 0) == 0) {
            const size_t split = text.find(' ', 9);
            if (split != std::string::npos) {
                int progress = std::atoi(text.substr(9, split - 9).c_str());
                set_status(UpdateState::Running, progress, text.substr(split + 1));
            }
        } else if (text.rfind("ERROR ", 0) == 0) {
            error = text.substr(6);
        } else if (!text.empty()) {
            last_output = text;
        }
    }
    if (output) fclose(output);

    int wait_status = 0;
    if (waitpid(pid, &wait_status, 0) < 0) {
        set_status(UpdateState::Failed, 0, "Could not collect updater result");
    } else if (WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0) {
        set_status(UpdateState::Succeeded, 100, "Update installed; restarting dashboard");
    } else {
        if (error.empty()) error = last_output.empty() ? "Update failed" : last_output;
        else if (!last_output.empty()) error += ": " + last_output;
        set_status(UpdateState::Failed, status().progress, error);
    }
}
