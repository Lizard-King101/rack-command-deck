#pragma once
#include "metrics_store.h"
#include "activity_store.h"
#include <libwebsockets.h>
#include <string>
#include <cstdint>

class WsServer {
public:
    WsServer(MetricsStore& store, ActivityStore& activity, uint16_t port);
    ~WsServer();

    // Runs the libwebsockets event loop. Blocks until stop() is called.
    void run();
    void stop();

    // Send a command to the agent identified by hostname.
    // Returns false if the host isn't connected.
    bool send_command(const std::string& host, const std::string& json_msg);

    // Public so the file-scope lws_protocols[] table can reference it.
    static int lws_callback(lws* wsi, lws_callback_reasons reason,
                            void* user, void* in, size_t len);

private:

    MetricsStore&   store_;
    ActivityStore&  activity_;
    uint16_t        port_;
    lws_context*    ctx_    = nullptr;
    bool            running_ = true;
};
