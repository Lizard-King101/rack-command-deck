#pragma once
#include "config.h"
#include "control/handler.h"
#include <libwebsockets.h>
#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <functional>

class WsClient {
public:
    WsClient(const AgentConfig::Connection& cfg,
             ControlHandler& handler,
             std::function<void()> on_connect = nullptr);
    ~WsClient();

    void run();
    void stop();
    // Thread-safe: callable from any thread including the lws callback thread.
    void enqueue(const std::string& json_msg);

    static int lws_callback(lws* wsi, lws_callback_reasons reason,
                             void* user, void* in, size_t len);
private:
    void flush_one();

    AgentConfig::Connection       cfg_;
    ControlHandler&               handler_;
    std::function<void()>         on_connect_;
    lws_context*                  ctx_      = nullptr;
    lws*                          wsi_      = nullptr;
    std::atomic<bool>             running_  = true;
    std::thread::id               lws_tid_; // set when run() starts

    std::mutex                    queue_mu_;
    std::queue<std::string>       queue_;
};
