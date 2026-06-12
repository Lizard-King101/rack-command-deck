#include "command_router.h"
#include "wol.h"
#include <nlohmann/json.hpp>
#include <chrono>

CommandRouter::CommandRouter(SynlinkClient* pdu, ActivityStore& activity, SendFn send_fn)
    : pdu_(pdu), activity_(activity), send_(std::move(send_fn)) {}

std::string CommandRouter::make_id(const std::string& host, const std::string& action) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return (host.empty() ? "deck" : host) + "_" + action + "_"
         + std::to_string(ms) + "_" + std::to_string(sequence_++);
}

std::string CommandRouter::dispatch(protocol::CommandMessage cmd,
                                    const std::string& target_host,
                                    int pdu_outlet) {
    cmd.id = make_id(target_host, cmd.action);
    std::string detail;
    if (cmd.action == "service")
        detail = cmd.service_name + " " + cmd.service_op;
    else if (cmd.action == "script")
        detail = cmd.service_op.empty() ? cmd.service_name : cmd.service_op;
    else if (cmd.action == "outlet")
        detail = "outlet " + std::to_string(pdu_outlet)
               + (cmd.outlet_state ? " on" : " off");
    activity_.begin(cmd, target_host, detail);

    if (cmd.action == "outlet") {
        bool ok = pdu_ && pdu_outlet > 0 && pdu_->set_outlet(pdu_outlet, cmd.outlet_state);
        activity_.complete({"cmd_result", cmd.id, ok,
            ok ? "outlet updated" : "PDU command failed"});
        return cmd.id;
    }
    if (cmd.action == "wol") {
        bool ok = send_wol(cmd.service_name);
        activity_.complete({"cmd_result", cmd.id, ok,
            ok ? "magic packet sent" : "failed to send magic packet"});
        return cmd.id;
    }

    nlohmann::json j = cmd;
    if (!send_(target_host, j.dump()))
        activity_.complete({"cmd_result", cmd.id, false, "host is not connected"});
    return cmd.id;
}

std::string CommandRouter::dispatch_wol(const std::string& host, const std::string& mac) {
    protocol::CommandMessage cmd;
    cmd.action = "wol";
    cmd.service_name = mac;
    return dispatch(cmd, host, 0);
}
