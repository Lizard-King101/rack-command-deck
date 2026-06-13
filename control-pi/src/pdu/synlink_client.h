#pragma once
#include "protocol.h"
#include "config.h"
#include <mutex>
#include <optional>
#include <unordered_map>
#include <string>

// Synlink Smart PDU REST client.
// API base: http://<host>/api
class SynlinkClient {
public:
    explicit SynlinkClient(const Config::Pdu& cfg);

    // Returns outlet state and rack-level inlet current. Nullopt on error.
    std::optional<protocol::PduSnapshot> get_snapshot();

    // Set outlet on (true) or off (false). Returns true on success.
    bool set_outlet(int outlet, bool on);

private:
    struct HttpResponse {
        long        status = 0;
        std::string body;
    };

    HttpResponse request(const std::string& method, const std::string& url,
                         const std::string& body = {});
    HttpResponse api_request(const std::string& method, const std::string& path,
                             const std::string& body = {});

    std::string base_url_;
    std::string access_token_;
    float nominal_voltage_;
    std::mutex outlet_ids_mutex_;
    std::unordered_map<int, std::string> outlet_ids_;
};
