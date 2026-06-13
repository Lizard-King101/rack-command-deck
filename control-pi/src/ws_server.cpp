#include "ws_server.h"
#include <nlohmann/json.hpp>
#include <map>
#include <mutex>
#include <deque>
#include <cstdio>

using json = nlohmann::json;

struct SessionData {
    std::string host;
    std::string rx;
    std::deque<std::string> pending_tx;
};

static std::map<lws*, SessionData>  sessions;
static std::mutex                   sessions_mu;
static WsServer*                    g_server = nullptr;

static lws_protocols protocols[] = {
    {
        "deck-protocol",
        WsServer::lws_callback,
        0,
        65536,
        0, nullptr, 0
    },
    {}
};

WsServer::WsServer(MetricsStore& store, ActivityStore& activity, uint16_t port)
    : store_(store), activity_(activity), port_(port) {
    g_server = this;
    lws_context_creation_info info{};
    info.port      = port_;
    info.protocols = protocols;
    info.options   = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    ctx_ = lws_create_context(&info);
    if (!ctx_) fprintf(stderr, "[server] failed to create lws context\n");
}

WsServer::~WsServer() {
    if (ctx_) lws_context_destroy(ctx_);
}

void WsServer::run() {
    fprintf(stderr, "[server] listening on port %d\n", port_);
    while (running_)
        lws_service(ctx_, 100);
}

void WsServer::stop() { running_ = false; }

bool WsServer::send_command(const std::string& host, const std::string& msg) {
    std::lock_guard<std::mutex> lk(sessions_mu);
    for (auto& [wsi, sess] : sessions) {
        if (sess.host == host) {
            sess.pending_tx.push_back(msg);
            lws_callback_on_writable(wsi);
            return true;
        }
    }
    return false;
}

int WsServer::lws_callback(lws* wsi, lws_callback_reasons reason,
                            void* /*user*/, void* in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED: {
        fprintf(stderr, "[server] client connected\n");
        std::lock_guard<std::mutex> lk(sessions_mu);
        sessions[wsi] = SessionData{};
        break;
    }
    case LWS_CALLBACK_CLOSED: {
        std::string host;
        {
            std::lock_guard<std::mutex> lk(sessions_mu);
            auto it = sessions.find(wsi);
            if (it != sessions.end()) { host = it->second.host; sessions.erase(it); }
        }
        fprintf(stderr, "[server] client disconnected: %s\n", host.empty() ? "(unknown)" : host.c_str());
        if (!host.empty()) g_server->store_.mark_offline(host);
        break;
    }
    case LWS_CALLBACK_RECEIVE: {
        std::string raw;
        {
            std::lock_guard<std::mutex> lk(sessions_mu);
            auto it = sessions.find(wsi);
            if (it == sessions.end()) break;
            it->second.rx.append((char*)in, len);
            if (!lws_is_final_fragment(wsi) || lws_remaining_packet_payload(wsi) > 0)
                break;
            raw = std::move(it->second.rx);
            it->second.rx.clear();
        }
        try {
            auto j    = json::parse(raw);
            auto type = j.value("type", "");

            if (type == "hello") {
                auto hello = j.get<protocol::HelloMessage>();
                fprintf(stderr, "[server] hello from %s (outlet %d)\n",
                        hello.host.c_str(), hello.outlet);
                g_server->store_.update_hello(hello);
                std::lock_guard<std::mutex> lk(sessions_mu);
                sessions[wsi].host = hello.host;
            } else if (type == "metrics") {
                g_server->store_.update_metrics(j.get<protocol::Metrics>());
            } else if (type == "cmd_result") {
                g_server->activity_.complete(j.get<protocol::CommandResult>());
                fprintf(stderr, "[server] cmd_result id=%s ok=%d\n",
                        j.value("id","?").c_str(), (int)j.value("ok", false));
            } else {
                fprintf(stderr, "[server] unknown message type: '%s'\n", type.c_str());
            }
        } catch (const std::exception& e) {
            fprintf(stderr, "[server] parse error: %s\n", e.what());
        }
        break;
    }
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        std::string msg;
        {
            std::lock_guard<std::mutex> lk(sessions_mu);
            auto it = sessions.find(wsi);
            if (it == sessions.end() || it->second.pending_tx.empty()) break;
            msg = std::move(it->second.pending_tx.front());
            it->second.pending_tx.pop_front();
        }
        std::vector<uint8_t> buf(LWS_PRE + msg.size());
        memcpy(buf.data() + LWS_PRE, msg.data(), msg.size());
        lws_write(wsi, buf.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
        {
            std::lock_guard<std::mutex> lk(sessions_mu);
            auto it = sessions.find(wsi);
            if (it != sessions.end() && !it->second.pending_tx.empty())
                lws_callback_on_writable(wsi);
        }
        break;
    }
    default: break;
    }
    return 0;
}
