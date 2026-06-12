#pragma once
#include "protocol.h"
#include "config.h"
#include <vector>
#include <string>

// Synlink Smart PDU REST client.
// API base: http://<host>/api/v1
// Verify exact endpoint paths against your PDU firmware docs.
class SynlinkClient {
public:
    explicit SynlinkClient(const Config::Pdu& cfg);

    // Returns state for all 8 outlets. Empty on error.
    std::vector<protocol::OutletState> get_outlets();

    // Set outlet on (true) or off (false). Returns true on success.
    bool set_outlet(int outlet, bool on);

private:
    std::string get(const std::string& path);
    std::string post(const std::string& path, const std::string& body);

    std::string base_url_;
    std::string username_;
    std::string password_;
};
