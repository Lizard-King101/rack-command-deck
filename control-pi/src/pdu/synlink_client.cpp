#include "synlink_client.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

namespace {
size_t write_cb(char* ptr, size_t size, size_t nmemb, std::string* buf) {
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}
} // namespace

SynlinkClient::SynlinkClient(const Config::Pdu& cfg)
    : base_url_("http://" + cfg.host + ":" + std::to_string(cfg.port) + "/api/v1")
    , username_(cfg.username)
    , password_(cfg.password)
{}

std::string SynlinkClient::get(const std::string& path) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string resp;
    std::string url  = base_url_ + path;
    std::string auth = username_ + ":" + password_;
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD,        auth.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return resp;
}

std::string SynlinkClient::post(const std::string& path, const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    std::string resp;
    std::string url  = base_url_ + path;
    std::string auth = username_ + ":" + password_;
    struct curl_slist* hdrs = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD,        auth.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L);
    curl_easy_perform(curl);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return resp;
}

std::vector<protocol::OutletState> SynlinkClient::get_outlets() {
    // TODO: verify endpoint path against your specific Synlink firmware version.
    // Common paths: /api/v1/outlets  or  /api/v1/pdu/outlets
    auto raw = get("/outlets");
    if (raw.empty()) return {};
    try {
        auto j = json::parse(raw);
        std::vector<protocol::OutletState> out;
        // Expected response: array of outlet objects
        // Adjust field names to match your firmware's actual JSON keys
        for (auto& o : j) {
            protocol::OutletState s;
            s.outlet = o.value("index",    0);
            s.name   = o.value("name",     "");
            s.on     = o.value("state",    "off") == "on";
            s.watts  = o.value("watt",     0.f);
            s.amps   = o.value("current",  0.f);
            s.volts  = o.value("voltage",  0.f);
            out.push_back(s);
        }
        return out;
    } catch (...) { return {}; }
}

bool SynlinkClient::set_outlet(int outlet, bool on) {
    // TODO: verify endpoint + body format against your firmware docs
    std::string path = "/outlets/" + std::to_string(outlet);
    std::string body = json{{"state", on ? "on" : "off"}}.dump();
    auto resp = post(path, body);
    if (resp.empty()) return false;
    try {
        auto j = json::parse(resp);
        return j.value("success", false);
    } catch (...) { return false; }
}
