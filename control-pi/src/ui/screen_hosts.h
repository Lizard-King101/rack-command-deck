#pragma once
#include <lvgl.h>
#include "metrics_store.h"
#include "pdu/pdu_store.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

class ScreenHosts {
public:
    using DetailCb = std::function<void(const std::string& host)>;

    ScreenHosts(lv_obj_t* parent, DetailCb on_detail);

    lv_obj_t* root() const { return container_; }
    void show() { lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN); }
    void hide() { lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN); }

    void refresh(const std::map<std::string, HostEntry>& hosts,
                 const std::vector<protocol::OutletState>& outlets,
                 bool pdu_enabled);

private:
    lv_obj_t* container_   = nullptr;
    lv_obj_t* scroll_cont_ = nullptr;
    DetailCb  on_detail_;
};
