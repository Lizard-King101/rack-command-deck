#include "service_ctrl.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <sys/wait.h>

static std::pair<std::string, int> run(const std::string& cmd) {
    std::array<char, 512> buf;
    std::string out;
    bool truncated = false;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {"", -1};
    while (fgets(buf.data(), buf.size(), pipe)) {
        if (out.size() < 4096) {
            out += buf.data();
            if (out.size() > 4096) {
                out.resize(4096);
                truncated = true;
            }
        } else {
            truncated = true;
        }
    }
    if (truncated) out += "\n[output truncated]";
    return {out, pclose(pipe)};
}

protocol::CommandResult service_ctrl::manage(const std::string& cmd_id,
                                              const std::string& service,
                                              const std::string& op) {
    static const std::array<const char*, 4> valid_ops = {"start","stop","restart","status"};
    bool valid = false;
    for (auto v : valid_ops) if (op == v) { valid = true; break; }
    if (!valid)
        return { "cmd_result", cmd_id, false, "unknown op: " + op };

    std::string cmd = "systemctl " + op + " " + service + " 2>&1";
    auto [out, status] = run(cmd);
    bool ok = status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (out.empty()) out = ok ? "service command completed" : "service command failed";
    return { "cmd_result", cmd_id, ok, out };
}
