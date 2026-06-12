#include "script_ctrl.h"
#include <array>
#include <cstdio>
#include <sys/wait.h>

protocol::CommandResult script_ctrl::run(const std::string& cmd_id,
                                         const std::string& command) {
    std::array<char, 512> buf{};
    std::string output;
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (!pipe)
        return {"cmd_result", cmd_id, false, "failed to start script"};

    bool truncated = false;
    while (fgets(buf.data(), buf.size(), pipe)) {
        if (output.size() < 4096) {
            output += buf.data();
            if (output.size() > 4096) {
                output.resize(4096);
                truncated = true;
            }
        } else {
            truncated = true;
        }
    }
    int status = pclose(pipe);
    bool ok = status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (truncated) output += "\n[output truncated]";
    if (output.empty()) output = ok ? "script completed" : "script failed";
    return {"cmd_result", cmd_id, ok, output};
}
