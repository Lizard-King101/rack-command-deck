#pragma once
#include <lvgl.h>
#include "config.h"
#include "metrics_store.h"
#include "activity_store.h"
#include <map>
#include <string>

class ScreenSettings {
public:
    ScreenSettings(lv_obj_t* parent, const Config& cfg, ActivityStore& activity);

    lv_obj_t* root() const { return container_; }
    void show() { lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN); }
    void hide() { lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN); }

    void refresh(const std::map<std::string, HostEntry>& hosts);

private:
    lv_obj_t*    container_ = nullptr;
    lv_obj_t*    lbl_agents_ = nullptr;
    lv_obj_t*    lbl_activity_ = nullptr;
    const Config& cfg_;
    ActivityStore& activity_;
};
