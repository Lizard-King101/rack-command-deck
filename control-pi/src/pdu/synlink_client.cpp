#include "synlink_client.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

namespace {
size_t write_cb(char* ptr, size_t size, size_t nmemb, std::string* buf) {
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

} // namespace

SynlinkClient::SynlinkClient(const Config::Pdu& cfg)
    : base_url_("http://" + cfg.host + ":" + std::to_string(cfg.port) + "/api")
    , access_token_(cfg.access_token)
    , nominal_voltage_(cfg.nominal_voltage)
{}

SynlinkClient::HttpResponse SynlinkClient::request(
        const std::string& method, const std::string& url,
        const std::string& body) {
    HttpResponse response;
    CURL* curl = curl_easy_init();
    if (!curl) return response;

    struct curl_slist* headers = nullptr;
    if (!body.empty())
        headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!access_token_.empty()) {
        const std::string authorization = "Authorization: Bearer " + access_token_;
        headers = curl_slist_append(headers, authorization.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    if (method != "GET")
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    if (!body.empty())
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

    if (curl_easy_perform(curl) == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

SynlinkClient::HttpResponse SynlinkClient::api_request(
        const std::string& method, const std::string& path, const std::string& body) {
    return request(method, base_url_ + path, body);
}

std::optional<protocol::PduSnapshot> SynlinkClient::get_snapshot() {
    auto outlet_response = api_request("GET", "/outlets");
    if (outlet_response.status < 200 || outlet_response.status >= 300 ||
        outlet_response.body.empty()) return std::nullopt;
    try {
        protocol::PduSnapshot snapshot;
        std::unordered_map<int, std::string> ids;
        for (auto& o : json::parse(outlet_response.body)) {
            protocol::OutletState s;
            s.outlet = o.value("outletIndex", o.value("index", 0));
            s.name   = o.value("outletName", o.value("name", ""));
            auto state = o.value("state", "OFF");
            std::transform(state.begin(), state.end(), state.begin(),
                           [](unsigned char c) { return std::toupper(c); });
            s.on     = state == "ON";
            snapshot.outlets.push_back(s);
            if (s.outlet > 0 && o.contains("id") && o["id"].is_string())
                ids[s.outlet] = o["id"].get<std::string>();
        }

        auto inlet_response = api_request("GET", "/inlets");
        if (inlet_response.status >= 200 && inlet_response.status < 300 &&
            !inlet_response.body.empty()) {
            bool found_current = false;
            for (auto& inlet : json::parse(inlet_response.body)) {
                if (!inlet.contains("inletCurrentRms") || !inlet["inletCurrentRms"].is_number())
                    continue;
                snapshot.inlet_amps += inlet["inletCurrentRms"].get<float>();
                found_current = true;
            }
            if (found_current && nominal_voltage_ > 0.f) {
                snapshot.nominal_volts = nominal_voltage_;
                snapshot.estimated_watts = snapshot.inlet_amps * nominal_voltage_;
                snapshot.measurements_available = true;
            }
        }
        {
            std::lock_guard lock(outlet_ids_mutex_);
            outlet_ids_ = std::move(ids);
        }
        return snapshot;
    } catch (...) { return std::nullopt; }
}

bool SynlinkClient::set_outlet(int outlet, bool on) {
    std::string id;
    {
        std::lock_guard lock(outlet_ids_mutex_);
        if (auto it = outlet_ids_.find(outlet); it != outlet_ids_.end()) id = it->second;
    }
    if (id.empty()) {
        get_snapshot();
        std::lock_guard lock(outlet_ids_mutex_);
        if (auto it = outlet_ids_.find(outlet); it != outlet_ids_.end()) id = it->second;
    }
    if (id.empty()) return false;

    auto body = json{{"id", id}, {"state", on ? "ON" : "OFF"}}.dump();
    auto response = api_request("PUT", "/outlets/" + id, body);
    return response.status >= 200 && response.status < 300;
}
