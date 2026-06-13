#pragma once
#include <lvgl.h>
#include "metrics_store.h"
#include "pdu/pdu_store.h"
#include "command_router.h"
#include "activity_store.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

class ScreenHostDetail {
public:
    using BackCb = std::function<void()>;

    enum KbTarget { KB_NONE, KB_NAME, KB_MAC };

    // Public so file-local make_btn helper and keyboard cb can dispatch
    struct CbData {
        enum Type {
            REBOOT, SHUTDOWN, GRACEFUL, WOL, OUTLET, SERVICE, SCRIPT,
            KB_SHOW_NAME, KB_SHOW_MAC,
            PDU_DEC, PDU_INC, PDU_SAVE, PDU_CLEAR,
            REMOVE_HOST, REMOVE_CONFIRM, REMOVE_CANCEL
        };
        ScreenHostDetail* self;
        Type type;
        std::string str1;
        std::string str2;
    };

    struct KbCbData { ScreenHostDetail* self; };

    ScreenHostDetail(lv_obj_t* parent, MetricsStore& store,
                     PduStore& pdu, CommandRouter& router, ActivityStore& activity,
                     BackCb on_back);
    ~ScreenHostDetail();

    lv_obj_t* root() const { return container_; }
    void show(const std::string& host);
    void hide();
    void refresh();
    bool is_visible() const { return visible_; }

    // Action handlers — public so CbData dispatch can call them
    void on_reboot();
    void on_shutdown();
    void on_shutdown_graceful();
    void on_wol();
    void on_outlet_toggle();
    void on_service(const std::string& svc, const std::string& op);
    void on_script(const std::string& name, const std::string& command);

    // CRUD actions
    void show_keyboard(KbTarget target);
    void hide_keyboard(bool save);
    void on_pdu_dec();
    void on_pdu_inc();
    void on_pdu_save();
    void on_pdu_clear();
    void on_remove_host();
    void on_remove_confirm();
    void on_remove_cancel();

    BackCb on_back_;

private:
    void rebuild(const std::string& host);
    void build_stats_section(lv_obj_t* cont, const HostEntry& e);
    void build_power_section(lv_obj_t* cont, const HostEntry& e);
    void build_chart_section(lv_obj_t* cont);
    void build_service_script_section(lv_obj_t* cont, const HostEntry& e);
    void build_activity_section(lv_obj_t* cont);
    void build_machine_config_section(lv_obj_t* cont, const HostEntry& e);

    void update_stats(const HostEntry& e);
    void update_charts(const HostEntry& e);
    void update_controls(const HostEntry& e);
    void update_activity();
    void update_pdu_override_label();

    void save_display_name();
    void save_mac();

    void poll_graceful_shutdown();
    void reset_shutdown_state();

    MetricsStore&  store_;
    PduStore&      pdu_;
    CommandRouter& router_;
    ActivityStore& activity_;

    lv_obj_t* container_   = nullptr;
    lv_obj_t* scroll_cont_ = nullptr;
    std::string current_host_;
    bool        visible_ = false;

    lv_obj_t* lbl_os_        = nullptr;
    lv_obj_t* lbl_uptime_    = nullptr;
    lv_obj_t* lbl_load_      = nullptr;
    lv_obj_t* bar_temp_      = nullptr;
    lv_obj_t* lbl_temp_      = nullptr;
    lv_obj_t* bar_cpu_       = nullptr;
    lv_obj_t* lbl_cpu_val_   = nullptr;
    lv_obj_t* bar_ram_       = nullptr;
    lv_obj_t* lbl_ram_val_   = nullptr;
    lv_obj_t* lbl_disks_     = nullptr;
    lv_obj_t* lbl_net_       = nullptr;

    lv_obj_t* chart_cpu_     = nullptr;
    lv_obj_t* chart_temp_    = nullptr;
    lv_obj_t* chart_ram_     = nullptr;
    lv_chart_series_t* ser_cpu_  = nullptr;
    lv_chart_series_t* ser_temp_ = nullptr;
    lv_chart_series_t* ser_ram_  = nullptr;

    std::array<lv_coord_t, HISTORY_LEN> arr_cpu_{};
    std::array<lv_coord_t, HISTORY_LEN> arr_temp_{};
    std::array<lv_coord_t, HISTORY_LEN> arr_ram_{};

    lv_obj_t* btn_reboot_    = nullptr;
    lv_obj_t* btn_shutdown_  = nullptr;
    lv_obj_t* btn_graceful_  = nullptr;
    lv_obj_t* btn_wol_       = nullptr;
    lv_obj_t* btn_outlet_    = nullptr;
    lv_obj_t* lbl_outlet_    = nullptr;
    lv_obj_t* lbl_power_summary_ = nullptr;
    lv_obj_t* lbl_shutdown_status_ = nullptr;
    lv_obj_t* lbl_activity_        = nullptr;

    // CRUD widgets
    lv_obj_t* ta_name_              = nullptr;
    lv_obj_t* ta_mac_               = nullptr;
    lv_obj_t* lbl_pdu_override_val_ = nullptr;
    lv_obj_t* lbl_remove_confirm_   = nullptr;
    lv_obj_t* confirm_row_          = nullptr;
    lv_obj_t* kb_                   = nullptr;
    KbTarget  kb_target_            = KB_NONE;
    int       pdu_override_pending_ = -1;

    PowerState shutdown_state_ = PowerState::Normal;
    std::chrono::steady_clock::time_point shutdown_started_;
    lv_timer_t* shutdown_poll_timer_ = nullptr;

    std::vector<std::unique_ptr<CbData>>   cb_data_;
    std::unique_ptr<KbCbData>              kb_cb_data_;
};
