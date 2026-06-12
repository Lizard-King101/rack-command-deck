#include "screen_pdu.h"
#include "styles.h"
#include <algorithm>
#include <cstdio>
#include <ctime>

namespace {
struct ButtonData { std::function<void()> cb; };
struct OutletData { PduStore* pdu; int outlet; ScreenPdu::ToggleCb cb; };
}

ScreenPdu::ScreenPdu(lv_obj_t* parent, PduStore& pdu, ToggleCb on_toggle, bool enabled,
                     const Config& cfg, PowerHistoryStore& history,
                     PowerSequenceEngine& sequences, PowerBudgetController& budgets)
    : pdu_(pdu), on_toggle_(std::move(on_toggle)), enabled_(enabled), cfg_(cfg),
      history_(history), sequences_(sequences), budgets_(budgets) {
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, 800, 390);
    lv_obj_set_style_bg_color(container_, styles::BG, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 8, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    styles::make_static(container_);

    lv_obj_t* nav = lv_obj_create(container_);
    lv_obj_set_size(nav, 784, 36);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(nav, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 0, 0);
    lv_obj_set_style_pad_column(nav, 6, 0);
    styles::make_static(nav);
    const char* labels[] = {"SUMMARY", "OUTLETS", "HISTORY", "SEQUENCES"};
    for (int i = 0; i < VIEW_COUNT; ++i)
        make_button(nav, labels[i], 190, [this, i] { show_view((View)i); });

    for (int i = 0; i < VIEW_COUNT; ++i) {
        views_[i] = lv_obj_create(container_);
        lv_obj_set_size(views_[i], 784, 330);
        lv_obj_set_pos(views_[i], 0, 40);
        lv_obj_set_style_bg_opa(views_[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(views_[i], 0, 0);
        lv_obj_set_style_pad_all(views_[i], 0, 0);
        styles::make_static(views_[i]);
    }
    build_summary();
    build_outlets();
    build_history();
    build_sequences();

    confirm_overlay_ = lv_obj_create(container_);
    lv_obj_set_size(confirm_overlay_, 430, 160);
    lv_obj_align(confirm_overlay_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(confirm_overlay_, &styles::panel, 0);
    lv_obj_add_flag(confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
    confirm_label_ = lv_label_create(confirm_overlay_);
    lv_obj_add_style(confirm_label_, &styles::label_value, 0);
    lv_obj_set_width(confirm_label_, 390);
    lv_label_set_long_mode(confirm_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(confirm_label_, 0, 5);
    make_button(confirm_overlay_, "CONFIRM", 130, [this] { confirm(); });
    auto* yes = lv_obj_get_child(confirm_overlay_, 1);
    lv_obj_align(yes, LV_ALIGN_BOTTOM_LEFT, 15, -5);
    make_button(confirm_overlay_, "CANCEL", 130, [this] {
        confirm_action_ = NONE;
        lv_obj_add_flag(confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
    });
    auto* cancel = lv_obj_get_child(confirm_overlay_, 2);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, -15, -5);
    show_view(SUMMARY);
}

lv_obj_t* ScreenPdu::make_button(lv_obj_t* parent, const char* text, int width,
                                 std::function<void()> cb) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, width, 32);
    lv_obj_add_style(btn, &styles::btn_action, 0);
    lv_obj_add_style(btn, &styles::btn_action_pressed, LV_STATE_PRESSED);
    styles::make_static(btn);
    auto* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    auto* data = new ButtonData{std::move(cb)};
    lv_obj_add_event_cb(btn, +[](lv_event_t* event) {
        static_cast<ButtonData*>(lv_event_get_user_data(event))->cb();
    }, LV_EVENT_CLICKED, data);
    return btn;
}

void ScreenPdu::show_view(View view) {
    active_view_ = view;
    for (int i = 0; i < VIEW_COUNT; ++i)
        if (i == view) lv_obj_clear_flag(views_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(views_[i], LV_OBJ_FLAG_HIDDEN);
}

void ScreenPdu::build_summary() {
    auto make_panel = [&](int x, int y, int w, int h, const char* title, lv_obj_t** value) {
        auto* panel = lv_obj_create(views_[SUMMARY]);
        lv_obj_set_size(panel, w, h);
        lv_obj_set_pos(panel, x, y);
        lv_obj_add_style(panel, &styles::panel, 0);
        styles::make_static(panel);
        auto* caption = lv_label_create(panel);
        lv_obj_add_style(caption, &styles::label_secondary, 0);
        lv_label_set_text(caption, title);
        *value = lv_label_create(panel);
        lv_obj_add_style(*value, &styles::label_title, 0);
        lv_obj_set_pos(*value, 0, 25);
    };
    make_panel(0, 0, 250, 110, "CURRENT RACK LOAD", &lbl_load_);
    make_panel(258, 0, 250, 110, "ESTIMATED ENERGY / COST", &lbl_energy_);
    make_panel(516, 0, 268, 110, "PDU HEALTH", &lbl_health_);
    auto* budget = lv_obj_create(views_[SUMMARY]);
    lv_obj_set_size(budget, 784, 205);
    lv_obj_set_pos(budget, 0, 120);
    lv_obj_add_style(budget, &styles::panel, 0);
    styles::make_static(budget);
    lbl_budget_ = lv_label_create(budget);
    lv_obj_add_style(lbl_budget_, &styles::label_value, 0);
    budget_bar_ = lv_bar_create(budget);
    lv_obj_set_size(budget_bar_, 744, 34);
    lv_obj_set_pos(budget_bar_, 0, 38);
    lv_bar_set_range(budget_bar_, 0, std::max(1, (int)cfg_.power.critical_watts));
    lv_obj_add_style(budget_bar_, &styles::bar_track, 0);
    lv_obj_add_style(budget_bar_, &styles::bar_temp_ind, LV_PART_INDICATOR);
    auto* hint = lv_label_create(budget);
    lv_obj_add_style(hint, &styles::label_secondary, 0);
    lv_obj_set_pos(hint, 0, 92);
    char buf[180];
    snprintf(buf, sizeof(buf), "Warning %.0fW  |  Critical %.0fW  |  Auto shedding %s\nMissing polls are retained as data gaps.",
             cfg_.power.warning_watts, cfg_.power.critical_watts,
             budgets_.enabled() ? "enabled" : "disabled");
    lv_label_set_text(hint, buf);
}

void ScreenPdu::build_outlets() {
    lv_obj_set_flex_flow(views_[OUTLETS], LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(views_[OUTLETS], 6, 0);
    for (int i = 0; i < MAX_OUTLETS; ++i) {
        auto* tile = lv_obj_create(views_[OUTLETS]);
        lv_obj_set_size(tile, 386, 70);
        lv_obj_add_style(tile, &styles::card_wide, 0);
        styles::make_static(tile);
        auto* name = lv_label_create(tile);
        lv_obj_add_style(name, &styles::label_value, 0);
        lv_obj_set_pos(name, 0, 0);
        auto* metrics = lv_label_create(tile);
        lv_obj_add_style(metrics, &styles::label_secondary, 0);
        lv_obj_set_pos(metrics, 0, 25);
        auto* btn = lv_button_create(tile);
        lv_obj_set_size(btn, 65, 32);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_style(btn, &styles::btn_action, 0);
        auto* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, "OFF");
        lv_obj_center(lbl);
        auto* data = new OutletData{&pdu_, i + 1, on_toggle_};
        lv_obj_add_event_cb(btn, +[](lv_event_t* event) {
            auto* d = static_cast<OutletData*>(lv_event_get_user_data(event));
            auto outlet = d->pdu->get_outlet(d->outlet);
            if (d->cb) d->cb(d->outlet, !(outlet && outlet->on));
        }, LV_EVENT_CLICKED, data);
        tiles_[i] = tile;
    }
    lbl_total_ = lv_label_create(views_[OUTLETS]);
    lv_obj_add_style(lbl_total_, &styles::label_value, 0);
}

void ScreenPdu::build_history() {
    history_title_ = lv_label_create(views_[HISTORY]);
    lv_obj_add_style(history_title_, &styles::label_value, 0);
    lv_label_set_text(history_title_, "RACK POWER // 30 DAYS");
    auto* controls = lv_obj_create(views_[HISTORY]);
    lv_obj_set_size(controls, 500, 32);
    lv_obj_align(controls, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_style_pad_column(controls, 5, 0);
    make_button(controls, "24H", 70, [this] { history_days_ = 1; });
    make_button(controls, "7D", 70, [this] { history_days_ = 7; });
    make_button(controls, "30D", 70, [this] { history_days_ = 30; });
    make_button(controls, "NEXT OUTLET", 160, [this] {
        history_outlet_ = (history_outlet_ + 1) % (MAX_OUTLETS + 1);
    });
    history_chart_ = lv_chart_create(views_[HISTORY]);
    lv_obj_set_size(history_chart_, 784, 285);
    lv_obj_set_pos(history_chart_, 0, 32);
    lv_chart_set_type(history_chart_, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(history_chart_, history_points_.size());
    lv_chart_set_range(history_chart_, LV_CHART_AXIS_PRIMARY_Y, 0,
                       std::max(100, (int)cfg_.power.critical_watts));
    lv_obj_add_style(history_chart_, &styles::chart_bg, 0);
    history_series_ = lv_chart_add_series(history_chart_, styles::ACCENT2,
                                           LV_CHART_AXIS_PRIMARY_Y);
    history_points_.fill(LV_CHART_POINT_NONE);
    lv_chart_set_ext_y_array(history_chart_, history_series_, history_points_.data());
}

void ScreenPdu::build_sequences() {
    auto* controls = lv_obj_create(views_[SEQUENCES]);
    lv_obj_set_size(controls, 784, 48);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_style_pad_column(controls, 8, 0);
    make_button(controls, "START RACK", 180, [this] {
        show_confirmation(START_ALL, "Start the rack in dependency order?");
    });
    make_button(controls, "SHUT DOWN RACK", 180, [this] {
        show_confirmation(STOP_ALL, "Gracefully shut down the rack in reverse dependency order?");
    });
    make_button(controls, "AUTO SHEDDING", 180, [this] {
        if (budgets_.enabled()) budgets_.set_enabled(false);
        else show_confirmation(ENABLE_SHEDDING, "Enable conservative automatic load shedding?");
    });
    lbl_shed_ = lv_label_create(views_[SEQUENCES]);
    lv_obj_add_style(lbl_shed_, &styles::label_secondary, 0);
    lv_obj_set_pos(lbl_shed_, 0, 54);
    lbl_sequence_ = lv_label_create(views_[SEQUENCES]);
    lv_obj_add_style(lbl_sequence_, &styles::label_secondary, 0);
    lv_obj_set_size(lbl_sequence_, 784, 240);
    lv_obj_set_pos(lbl_sequence_, 0, 82);
    lv_label_set_long_mode(lbl_sequence_, LV_LABEL_LONG_WRAP);
}

void ScreenPdu::show_confirmation(ConfirmAction action, const char* text) {
    confirm_action_ = action;
    lv_label_set_text(confirm_label_, text);
    lv_obj_clear_flag(confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(confirm_overlay_);
}

void ScreenPdu::confirm() {
    std::string error;
    if (confirm_action_ == START_ALL) sequences_.start_all(true, &error);
    if (confirm_action_ == STOP_ALL) sequences_.start_all(false, &error);
    if (confirm_action_ == ENABLE_SHEDDING) budgets_.set_enabled(true);
    confirm_action_ = NONE;
    lv_obj_add_flag(confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
}

void ScreenPdu::refresh(const std::vector<protocol::OutletState>& outlets) {
    float total = 0;
    for (const auto& outlet : outlets) {
        total += outlet.watts;
        int idx = outlet.outlet - 1;
        if (idx < 0 || idx >= MAX_OUTLETS || !tiles_[idx]) continue;
        char buf[96];
        snprintf(buf, sizeof(buf), "%s", outlet.name.empty()
                 ? ("Outlet " + std::to_string(outlet.outlet)).c_str() : outlet.name.c_str());
        lv_label_set_text(lv_obj_get_child(tiles_[idx], 0), buf);
        snprintf(buf, sizeof(buf), "%.1fW  %.2fA  %.0fV", outlet.watts, outlet.amps, outlet.volts);
        lv_label_set_text(lv_obj_get_child(tiles_[idx], 1), buf);
        lv_label_set_text(lv_obj_get_child(lv_obj_get_child(tiles_[idx], 2), 0),
                          outlet.on ? "ON" : "OFF");
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "%.0f W", total);
    lv_label_set_text(lbl_load_, enabled_ ? buf : "--");
    lv_bar_set_value(budget_bar_, std::min((int)total, (int)cfg_.power.critical_watts), LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "Rack budget: %.0f / %.0f W%s", total, cfg_.power.critical_watts,
             budgets_.critical() ? "  CRITICAL" : budgets_.warning() ? "  WARNING" : "");
    lv_label_set_text(lbl_budget_, buf);
    auto analytics = history_.analytics(0, total);
    snprintf(buf, sizeof(buf), "%.2f kWh/day\n%.1f kWh/mo\n%s %.2f/mo",
             analytics.daily_kwh, analytics.monthly_kwh, cfg_.power.currency.c_str(),
             analytics.monthly_cost);
    lv_label_set_text(lbl_energy_, buf);
    lv_label_set_text(lbl_health_, pdu_.healthy() ? "CONNECTED" : "DISCONNECTED");
    snprintf(buf, sizeof(buf), "Total: %.1f W", total);
    lv_label_set_text(lbl_total_, buf);

    snprintf(buf, sizeof(buf), "%s // %d %s", history_outlet_ == 0 ? "RACK POWER" :
             ("OUTLET " + std::to_string(history_outlet_)).c_str(), history_days_,
             history_days_ == 1 ? "DAY" : "DAYS");
    lv_label_set_text(history_title_, buf);
    auto points = history_.history(history_outlet_,
        std::time(nullptr) - history_days_ * 86400LL, history_points_.size());
    history_points_.fill(LV_CHART_POINT_NONE);
    size_t offset = history_points_.size() > points.size() ? history_points_.size() - points.size() : 0;
    for (size_t i = 0; i < points.size() && offset + i < history_points_.size(); ++i)
        history_points_[offset + i] = (lv_coord_t)points[i].watts;
    lv_chart_refresh(history_chart_);

    auto status = sequences_.status();
    std::string sequence = status.name.empty() ? "No sequence has run." : status.name + "\n";
    for (const auto& step : status.steps) {
        const char* state = step.state == SequenceStepState::Pending ? "[ ]" :
                            step.state == SequenceStepState::Running ? "[>]" :
                            step.state == SequenceStepState::Succeeded ? "[OK]" : "[FAIL]";
        sequence += state + std::string(" ") + step.group + " / " + step.host;
        if (!step.detail.empty()) sequence += " - " + step.detail;
        sequence += "\n";
    }
    lv_label_set_text(lbl_sequence_, sequence.c_str());
    snprintf(buf, sizeof(buf), "Auto shedding: %s  |  Active: %s  |  Shed groups: %zu",
             budgets_.enabled() ? "enabled" : "disabled", budgets_.shedding() ? "yes" : "no",
             budgets_.shed_groups().size());
    lv_label_set_text(lbl_shed_, buf);
}
