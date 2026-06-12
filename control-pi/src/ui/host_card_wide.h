#pragma once
#include <lvgl.h>
#include "metrics_store.h"
#include "protocol.h"
#include <functional>
#include <optional>
#include <string>

class HostCardWide {
public:
    using TapCb = std::function<void(const std::string& host)>;

    static void update_or_create(lv_obj_t* parent,
                                  const std::string& host,
                                  const HostEntry& entry,
                                  std::optional<protocol::OutletState> outlet,
                                  bool pdu_enabled,
                                  TapCb on_tap);

private:
    static lv_obj_t* find_card(lv_obj_t* parent, const std::string& host);
    static lv_obj_t* create_card(lv_obj_t* parent, const std::string& host, TapCb on_tap);
    static void      populate(lv_obj_t* card, const HostEntry& entry,
                               std::optional<protocol::OutletState> outlet,
                               bool pdu_enabled);
};
