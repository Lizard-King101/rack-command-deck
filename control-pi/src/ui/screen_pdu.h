#pragma once
#include <lvgl.h>
#include "config.h"
#include "pdu/pdu_store.h"
#include "power/power_budget_controller.h"
#include "power/power_history_store.h"
#include "power/power_sequence_engine.h"
#include "protocol.h"
#include <array>
#include <functional>
#include <vector>

class ScreenPdu {
public:
    using ToggleCb = std::function<void(int outlet, bool new_state)>;

    ScreenPdu(lv_obj_t* parent, PduStore& pdu, ToggleCb on_toggle, bool enabled,
              const Config& cfg, PowerHistoryStore& history,
              PowerSequenceEngine& sequences, PowerBudgetController& budgets);

    lv_obj_t* root() const { return container_; }
    void show() { lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN); }
    void hide() { lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN); }
    void refresh(const std::vector<protocol::OutletState>& outlets);

private:
    enum View { SUMMARY, OUTLETS, HISTORY, SEQUENCES, VIEW_COUNT };
    enum ConfirmAction { NONE, START_ALL, STOP_ALL, ENABLE_SHEDDING };
    void show_view(View view);
    void show_confirmation(ConfirmAction action, const char* text);
    void confirm();
    lv_obj_t* make_button(lv_obj_t* parent, const char* text, int width,
                          std::function<void()> cb);
    void build_summary();
    void build_outlets();
    void build_history();
    void build_sequences();

    static constexpr int MAX_OUTLETS = 8;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* views_[VIEW_COUNT] = {};
    lv_obj_t* tiles_[MAX_OUTLETS] = {};
    lv_obj_t* lbl_load_ = nullptr;
    lv_obj_t* lbl_budget_ = nullptr;
    lv_obj_t* budget_bar_ = nullptr;
    lv_obj_t* lbl_energy_ = nullptr;
    lv_obj_t* lbl_health_ = nullptr;
    lv_obj_t* lbl_total_ = nullptr;
    lv_obj_t* lbl_sequence_ = nullptr;
    lv_obj_t* lbl_shed_ = nullptr;
    lv_obj_t* history_chart_ = nullptr;
    lv_obj_t* history_title_ = nullptr;
    lv_chart_series_t* history_series_ = nullptr;
    std::array<lv_coord_t, 120> history_points_{};
    lv_obj_t* confirm_overlay_ = nullptr;
    lv_obj_t* confirm_label_ = nullptr;
    ConfirmAction confirm_action_ = NONE;
    View active_view_ = SUMMARY;
    int history_outlet_ = 0;
    int history_days_ = 30;

    PduStore& pdu_;
    ToggleCb on_toggle_;
    bool enabled_;
    const Config& cfg_;
    PowerHistoryStore& history_;
    PowerSequenceEngine& sequences_;
    PowerBudgetController& budgets_;
    std::vector<std::function<void()>> callbacks_;
};
