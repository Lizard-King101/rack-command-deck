#include "ws_client.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>

using json = nlohmann::json;
static WsClient* g_client = nullptr;

static lws_protocols protocols[] = {
    { "deck-protocol", WsClient::lws_callback, 0, 65536, 0, nullptr, 0 },
    {}
};

WsClient::WsClient(const AgentConfig::Connection& cfg,
                   ControlHandler& handler,
                   std::function<void()> on_connect)
    : cfg_(cfg), handler_(handler), on_connect_(std::move(on_connect)) {
    g_client = this;
    lws_context_creation_info info{};
    info.port      = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    ctx_ = lws_create_context(&info);
}

WsClient::~WsClient() {
    if (ctx_) lws_context_destroy(ctx_);
}

void WsClient::run() {
    lws_tid_ = std::this_thread::get_id();
    while (running_) {
        fprintf(stderr, "[agent] connecting to %s:%d...\n",
                cfg_.control_host.c_str(), cfg_.control_port);

        lws_client_connect_info ci{};
        ci.context  = ctx_;
        ci.address  = cfg_.control_host.c_str();
        ci.port     = cfg_.control_port;
        ci.path     = "/";
        ci.host     = cfg_.control_host.c_str();
        ci.origin   = cfg_.control_host.c_str();
        ci.protocol = "deck-protocol";
        wsi_ = lws_client_connect_via_info(&ci);

        while (running_ && wsi_)
            lws_service(ctx_, 50);

        wsi_ = nullptr;
        if (running_) {
            fprintf(stderr, "[agent] disconnected, retrying in %dms\n",
                    cfg_.reconnect_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.reconnect_ms));
        }
    }
}

void WsClient::stop() { running_ = false; }

void WsClient::enqueue(const std::string& msg) {
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        queue_.push(msg);
    }
    if (std::this_thread::get_id() == lws_tid_) {
        // Already on the lws service thread (e.g. called from on_connect_ inside ESTABLISHED)
        if (wsi_) lws_callback_on_writable(wsi_);
    } else {
        // Foreign thread — wake the service loop
        if (ctx_) lws_cancel_service(ctx_);
    }
}

void WsClient::flush_one() {
    std::string msg;
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        if (queue_.empty()) return;
        msg = std::move(queue_.front());
        queue_.pop();
    }
    std::vector<uint8_t> buf(LWS_PRE + msg.size());
    memcpy(buf.data() + LWS_PRE, msg.data(), msg.size());
    lws_write(wsi_, buf.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);

    // If more messages are queued, request another writable callback immediately
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        if (!queue_.empty()) lws_callback_on_writable(wsi_);
    }
}

int WsClient::lws_callback(lws* wsi, lws_callback_reasons reason,
                             void* /*user*/, void* in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        fprintf(stderr, "[agent] connected\n");
        g_client->wsi_ = wsi;
        if (g_client->on_connect_) g_client->on_connect_();
        // If anything was queued before connect, flush it now
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        std::string raw((char*)in, len);
        try {
            auto j    = json::parse(raw);
            auto type = j.value("type", "");
            if (type == "cmd") {
                auto cmd    = j.get<protocol::CommandMessage>();
                auto result = g_client->handler_.dispatch(cmd);
                g_client->enqueue(json(result).dump());
            }
        } catch (...) {}
        break;
    }

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (g_client) g_client->flush_one();
        break;

    // lws_cancel_service fires this — use it to request a write if queued
    case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
        if (g_client && g_client->wsi_) {
            std::lock_guard<std::mutex> lk(g_client->queue_mu_);
            if (!g_client->queue_.empty())
                lws_callback_on_writable(g_client->wsi_);
        }
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        fprintf(stderr, "[agent] connection error: %s\n",
                in ? (char*)in : "unknown");
        g_client->wsi_ = nullptr;
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        fprintf(stderr, "[agent] connection closed\n");
        g_client->wsi_ = nullptr;
        break;

    default: break;
    }
    return 0;
}
