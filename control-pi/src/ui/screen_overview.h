#pragma once
#include <lvgl.h>
#include "metrics_store.h"
#include "pdu/pdu_store.h"
#include <functional>
#include <vector>
#include <string>

class ScreenOverview {
public:
    using TapCb = std::function<void(const std::string&)>;

    ScreenOverview(lv_obj_t* parent, TapCb on_tap);

    lv_obj_t* root() const { return container_; }
    void show()  { lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN); }
    void hide()  { lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN); }

    void refresh(const std::map<std::string, HostEntry>& hosts,
                 const std::vector<protocol::OutletState>& outlets,
                 bool pdu_enabled, bool power_warning = false,
                 bool power_critical = false);

private:
    void build_stats_panel(lv_obj_t* parent);
    void build_alert_bar(lv_obj_t* parent);
    void build_host_list(lv_obj_t* parent);

    lv_obj_t* container_       = nullptr;
    lv_obj_t* list_            = nullptr;

    // Stats panel widgets
    lv_obj_t* lbl_online_count_ = nullptr;
    lv_obj_t* lbl_avg_cpu_      = nullptr;
    lv_obj_t* lbl_avg_ram_      = nullptr;
    lv_obj_t* lbl_watts_        = nullptr;

    // Alert ticker
    lv_obj_t* alert_bar_  = nullptr;
    lv_obj_t* lbl_alert_  = nullptr;

    TapCb on_tap_;
};
