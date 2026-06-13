#pragma once
#include <lvgl.h>
#include "metrics_store.h"
#include "activity_store.h"
#include "pdu/pdu_store.h"
#include "command_router.h"
#include "config.h"
#include "power/power_history_store.h"
#include "power/power_sequence_engine.h"
#include "power/power_budget_controller.h"
#include "update_manager.h"
#include <memory>
#include <string>

class ScreenOverview;
class ScreenHosts;
class ScreenPdu;
class ScreenSettings;
class ScreenHostDetail;

class AppShell {
public:
    AppShell(MetricsStore& store, PduStore& pdu, ActivityStore& activity,
             CommandRouter& router, PowerHistoryStore& power_history,
             PowerSequenceEngine& sequences, PowerBudgetController& budgets,
             UpdateManager& updater, const Config& cfg);
    ~AppShell();

    void build();
    void refresh();

    enum Tab { TAB_OVERVIEW = 0, TAB_HOSTS, TAB_PDU, TAB_SETTINGS, TAB_COUNT };
    void show_tab(Tab t);
    void show_host_detail(const std::string& host);
    void pop_host_detail();

    void enter_detail_mode(const std::string& host, bool online);
    void exit_detail_mode();

    // Called by static LVGL timer callbacks — must be public
    void on_clock_tick()   { update_header_clock(); }
    void on_history_tick() { store_.tick_history(); }
    void on_stale_tick()   { store_.tick_online_status(15); }
    void on_cursor_tick();
    void on_power_tick() { budgets_.tick(); }

    // Accessed by cursor timer callback
    lv_obj_t* lbl_cursor_    = nullptr;
    bool      cursor_visible_ = true;

private:
    void build_header();
    void build_content_area();
    void build_tab_bar();
    void update_header_clock();
    void update_alert_badge();
    void update_activity_toast();

    MetricsStore&   store_;
    PduStore&       pdu_;
    ActivityStore&  activity_;
    CommandRouter&  router_;
    PowerHistoryStore& power_history_;
    PowerSequenceEngine& sequences_;
    PowerBudgetController& budgets_;
    UpdateManager& updater_;
    const Config&   cfg_;

    lv_obj_t* root_screen_  = nullptr;
    lv_obj_t* header_bar_   = nullptr;
    lv_obj_t* lbl_title_    = nullptr;
    lv_obj_t* lbl_clock_    = nullptr;
    lv_obj_t* lbl_badge_    = nullptr;
    lv_obj_t* content_area_ = nullptr;
    lv_obj_t* tab_bar_      = nullptr;
    lv_obj_t* activity_toast_ = nullptr;
    lv_obj_t* lbl_activity_toast_ = nullptr;
    lv_obj_t* tab_btns_[TAB_COUNT] = {};

    // Detail-mode header widgets (hidden in normal mode)
    lv_obj_t* btn_header_back_    = nullptr;
    lv_obj_t* lbl_detail_host_    = nullptr;
    lv_obj_t* lbl_detail_status_  = nullptr;

    Tab  active_tab_     = TAB_OVERVIEW;
    bool detail_visible_ = false;
    std::string detail_host_;
    std::string last_toast_id_;
    int         toast_ticks_ = 0;

    std::unique_ptr<ScreenOverview>   screen_overview_;
    std::unique_ptr<ScreenHosts>      screen_hosts_;
    std::unique_ptr<ScreenPdu>        screen_pdu_;
    std::unique_ptr<ScreenSettings>   screen_settings_;
    std::unique_ptr<ScreenHostDetail> screen_detail_;

    lv_timer_t* refresh_timer_ = nullptr;
    lv_timer_t* clock_timer_   = nullptr;
    lv_timer_t* history_timer_ = nullptr;
    lv_timer_t* stale_timer_   = nullptr;
    lv_timer_t* cursor_timer_  = nullptr;
    lv_timer_t* power_timer_   = nullptr;
};
