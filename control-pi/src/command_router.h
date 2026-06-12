#pragma once
#include "protocol.h"
#include "activity_store.h"
#include "pdu/synlink_client.h"
#include <atomic>
#include <string>
#include <functional>

// Determines whether a command goes to an agent over WebSocket or
// is handled locally (e.g. PDU outlet toggle).
class CommandRouter {
public:
    using SendFn = std::function<bool(const std::string& host, const std::string& json)>;

    CommandRouter(SynlinkClient* pdu, ActivityStore& activity, SendFn send_fn);

    // Dispatches cmd; pdu_outlet is the outlet assigned to the target host (0 = none).
    std::string dispatch(protocol::CommandMessage cmd,
                         const std::string& target_host,
                         int pdu_outlet);

    std::string dispatch_wol(const std::string& host, const std::string& mac);

private:
    std::string make_id(const std::string& host, const std::string& action);

    SynlinkClient* pdu_;
    ActivityStore& activity_;
    SendFn         send_;
    std::atomic<uint64_t> sequence_{0};
};
