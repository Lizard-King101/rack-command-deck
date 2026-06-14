#pragma once
#include <lvgl.h>
#include "config.h"
#include "metrics_store.h"
#include "activity_store.h"
#include "update_manager.h"
#include "theme_profile.h"
#include <functional>
#include <map>
#include <string>

class ScreenSettings {
public:
    ScreenSettings(lv_obj_t* parent, const Config& cfg, ActivityStore& activity,
                   UpdateManager& updater, ThemeStore& themes,
                   std::function<void(const ThemeProfile&)> on_apply);

    lv_obj_t* root() const { return container_; }
    void show() { lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN); }
    void hide() { lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN); }

    void refresh(const std::map<std::string, HostEntry>& hosts);
    void show_theme_studio();

private:
    void hide_theme_studio();
    void build_theme_studio(lv_obj_t* parent);
    void select_profile(size_t index);
    void refresh_theme_preview();
    void rebuild_profile_options();
    void cycle_accent();
    void cycle_status();
    void cycle_mark();
    void apply_profile();
    void duplicate_profile();
    void delete_profile();
    void export_profile();

    lv_obj_t*    container_ = nullptr;
    lv_obj_t*    main_page_ = nullptr;
    lv_obj_t*    theme_page_ = nullptr;
    lv_obj_t*    lbl_agents_ = nullptr;
    lv_obj_t*    lbl_activity_ = nullptr;
    lv_obj_t*    btn_update_ = nullptr;
    lv_obj_t*    bar_update_ = nullptr;
    lv_obj_t*    lbl_update_ = nullptr;
    lv_obj_t*    profile_dropdown_ = nullptr;
    lv_obj_t*    preview_ = nullptr;
    lv_obj_t*    preview_mark_ = nullptr;
    lv_obj_t*    preview_title_ = nullptr;
    lv_obj_t*    preview_meta_ = nullptr;
    lv_obj_t*    preview_accent_ = nullptr;
    lv_obj_t*    preview_ok_ = nullptr;
    lv_obj_t*    preview_warn_ = nullptr;
    lv_obj_t*    preview_danger_ = nullptr;
    lv_obj_t*    lbl_theme_status_ = nullptr;
    lv_obj_t*    lbl_active_theme_ = nullptr;
    lv_obj_t*    lbl_accent_value_ = nullptr;
    lv_obj_t*    lbl_status_value_ = nullptr;
    lv_obj_t*    lbl_mark_value_ = nullptr;
    lv_obj_t*    lbl_radius_value_ = nullptr;
    lv_obj_t*    lbl_shadow_value_ = nullptr;
    lv_obj_t*    lbl_density_value_ = nullptr;
    lv_obj_t*    lbl_motion_value_ = nullptr;
    lv_obj_t*    lbl_clock_value_ = nullptr;
    lv_obj_t*    lbl_screensaver_value_ = nullptr;
    lv_obj_t*    lbl_saver_background_value_ = nullptr;
    lv_obj_t*    lbl_saver_veil_value_ = nullptr;
    lv_obj_t*    lbl_saver_card_opacity_value_ = nullptr;
    lv_obj_t*    brand_title_ = nullptr;
    lv_obj_t*    brand_subtitle_ = nullptr;
    lv_obj_t*    theme_keyboard_ = nullptr;
    const Config& cfg_;
    ActivityStore& activity_;
    UpdateManager& updater_;
    ThemeStore& themes_;
    std::function<void(const ThemeProfile&)> on_apply_;
    std::vector<ThemeProfile> profiles_;
    ThemeProfile working_;
};
