#include "screen_host_detail.h"
#include "styles.h"
#include <cstdio>
#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ScreenHostDetail::~ScreenHostDetail() = default;

// Helper: make an action button and register a CbData dispatch callback
static lv_obj_t* make_btn(lv_obj_t* parent,
                            const char* label,
                            lv_style_t* base_style,
                            lv_style_t* pressed_style,
                            ScreenHostDetail::CbData* d) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_add_style(btn, base_style, 0);
    if (pressed_style) lv_obj_add_style(btn, pressed_style, LV_STATE_PRESSED);
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    styles::make_static(btn);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);

    lv_obj_add_event_cb(btn, +[](lv_event_t* e) {
        auto* d = static_cast<ScreenHostDetail::CbData*>(lv_event_get_user_data(e));
        switch (d->type) {
        case ScreenHostDetail::CbData::REBOOT:         d->self->on_reboot(); break;
        case ScreenHostDetail::CbData::SHUTDOWN:       d->self->on_shutdown(); break;
        case ScreenHostDetail::CbData::GRACEFUL:       d->self->on_shutdown_graceful(); break;
        case ScreenHostDetail::CbData::WOL:            d->self->on_wol(); break;
        case ScreenHostDetail::CbData::OUTLET:         d->self->on_outlet_toggle(); break;
        case ScreenHostDetail::CbData::SERVICE:        d->self->on_service(d->str1, d->str2); break;
        case ScreenHostDetail::CbData::SCRIPT:         d->self->on_script(d->str1, d->str2); break;
        case ScreenHostDetail::CbData::KB_SHOW_NAME:   d->self->show_keyboard(ScreenHostDetail::KB_NAME); break;
        case ScreenHostDetail::CbData::KB_SHOW_MAC:    d->self->show_keyboard(ScreenHostDetail::KB_MAC); break;
        case ScreenHostDetail::CbData::PDU_DEC:        d->self->on_pdu_dec(); break;
        case ScreenHostDetail::CbData::PDU_INC:        d->self->on_pdu_inc(); break;
        case ScreenHostDetail::CbData::PDU_SAVE:       d->self->on_pdu_save(); break;
        case ScreenHostDetail::CbData::PDU_CLEAR:      d->self->on_pdu_clear(); break;
        case ScreenHostDetail::CbData::REMOVE_HOST:    d->self->on_remove_host(); break;
        case ScreenHostDetail::CbData::REMOVE_CONFIRM: d->self->on_remove_confirm(); break;
        case ScreenHostDetail::CbData::REMOVE_CANCEL:  d->self->on_remove_cancel(); break;
        }
    }, LV_EVENT_CLICKED, d);

    return btn;
}

// ── Constructor ───────────────────────────────────────────────────────────────

ScreenHostDetail::ScreenHostDetail(lv_obj_t* parent, MetricsStore& store,
                                   PduStore& pdu, CommandRouter& router,
                                   ActivityStore& activity,
                                   BackCb on_back)
    : store_(store), pdu_(pdu), router_(router), activity_(activity),
      on_back_(std::move(on_back)) {
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, 800, 390);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, styles::BG, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    styles::make_static(container_);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

// ── Show / Hide ───────────────────────────────────────────────────────────────

void ScreenHostDetail::show(const std::string& host) {
    if (current_host_ != host) {
        current_host_ = host;
        rebuild(host);
    }
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    visible_ = true;
}

void ScreenHostDetail::hide() {
    // Hide keyboard if open
    if (kb_) lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    visible_ = false;
}

// ── Rebuild ───────────────────────────────────────────────────────────────────

void ScreenHostDetail::rebuild(const std::string& host) {
    if (shutdown_poll_timer_) {
        lv_timer_del(shutdown_poll_timer_);
        shutdown_poll_timer_ = nullptr;
    }
    shutdown_state_ = PowerState::Normal;

    cb_data_.clear();
    kb_cb_data_.reset();
    kb_target_ = KB_NONE;

    // Delete floating keyboard (child of container_, not scroll_cont_)
    if (kb_) { lv_obj_del(kb_); kb_ = nullptr; }

    if (scroll_cont_) { lv_obj_del(scroll_cont_); scroll_cont_ = nullptr; }

    // Null all widget pointers
    lbl_os_ = lbl_uptime_ = lbl_load_ = nullptr;
    bar_temp_ = lbl_temp_ = bar_cpu_ = lbl_cpu_val_ = bar_ram_ = lbl_ram_val_ = nullptr;
    lbl_disks_ = lbl_net_ = nullptr;
    chart_cpu_ = chart_temp_ = chart_ram_ = nullptr;
    ser_cpu_ = ser_temp_ = ser_ram_ = nullptr;
    btn_reboot_ = btn_shutdown_ = btn_graceful_ = btn_wol_ = btn_outlet_ = nullptr;
    lbl_outlet_ = lbl_shutdown_status_ = nullptr;
    ta_name_ = ta_mac_ = nullptr;
    lbl_pdu_override_val_ = nullptr;
    lbl_remove_confirm_ = confirm_row_ = nullptr;
    lbl_activity_ = nullptr;

    auto entry_opt = store_.get(host);
    if (!entry_opt) return;
    auto& e = *entry_opt;

    pdu_override_pending_ = e.pdu_outlet_override;

    scroll_cont_ = lv_obj_create(container_);
    lv_obj_set_size(scroll_cont_, 800, 390);
    lv_obj_set_pos(scroll_cont_, 0, 0);
    lv_obj_set_flex_flow(scroll_cont_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(scroll_cont_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_cont_, 0, 0);
    lv_obj_set_style_pad_all(scroll_cont_, 8, 0);
    lv_obj_set_style_pad_row(scroll_cont_, 8, 0);
    styles::make_vertical_scroll(scroll_cont_);

    build_stats_section(scroll_cont_, e);
    build_power_section(scroll_cont_, e);
    build_chart_section(scroll_cont_);
    build_service_script_section(scroll_cont_, e);
    build_activity_section(scroll_cont_);
    build_machine_config_section(scroll_cont_, e);

    // Floating keyboard — child of container_ so it overlays scroll_cont_
    kb_cb_data_ = std::make_unique<KbCbData>(KbCbData{this});
    kb_ = lv_keyboard_create(container_);
    lv_obj_set_size(kb_, 800, 220);
    lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb_, +[](lv_event_t* ev) {
        auto* kd = static_cast<ScreenHostDetail::KbCbData*>(lv_event_get_user_data(ev));
        kd->self->hide_keyboard(lv_event_get_code(ev) == LV_EVENT_READY);
    }, LV_EVENT_READY, kb_cb_data_.get());
    lv_obj_add_event_cb(kb_, +[](lv_event_t* ev) {
        auto* kd = static_cast<ScreenHostDetail::KbCbData*>(lv_event_get_user_data(ev));
        kd->self->hide_keyboard(false);
    }, LV_EVENT_CANCEL, kb_cb_data_.get());

    update_stats(e);
    update_charts(e);
    update_controls(e);
    update_activity();
    uint32_t count = lv_obj_get_child_count(scroll_cont_);
    for (uint32_t i = 0; i < count; ++i)
        styles::make_scroll_passthrough(lv_obj_get_child(scroll_cont_, i));
}

void ScreenHostDetail::build_activity_section(lv_obj_t* cont) {
    lv_obj_t* panel = lv_obj_create(cont);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_add_style(panel, &styles::panel, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 6, 0);
    styles::make_static(panel);

    lv_obj_t* hdr = lv_label_create(panel);
    lv_obj_add_style(hdr, &styles::label_value, 0);
    lv_obj_set_style_text_color(hdr, styles::TEXT, 0);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_label_set_text(hdr, "RECENT ACTIVITY");

    lbl_activity_ = lv_label_create(panel);
    lv_obj_add_style(lbl_activity_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_activity_, LV_PCT(100));
    lv_label_set_text(lbl_activity_, "No commands have been sent to this host.");
}

// ── Stats section ─────────────────────────────────────────────────────────────

void ScreenHostDetail::build_stats_section(lv_obj_t* cont, const HostEntry& e) {
    lv_obj_t* panel = lv_obj_create(cont);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, 164);
    lv_obj_add_style(panel, &styles::panel, 0);
    styles::make_static(panel);

    lv_obj_t* identity = lv_label_create(panel);
    lv_obj_add_style(identity, &styles::label_value, 0);
    lv_obj_set_style_text_color(identity, styles::TEXT, 0);
    lv_obj_set_pos(identity, 0, 0);
    lv_label_set_text(identity, "MACHINE HEALTH");

    lbl_os_ = lv_label_create(panel);
    lv_obj_add_style(lbl_os_, &styles::label_secondary, 0);
    lv_obj_set_pos(lbl_os_, 0, 23);
    lv_obj_set_width(lbl_os_, 420);
    lv_label_set_long_mode(lbl_os_, LV_LABEL_LONG_DOT);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s  %s", e.arch.c_str(), e.os.c_str());
    lv_label_set_text(lbl_os_, buf);

    lbl_uptime_ = lv_label_create(panel);
    lv_obj_add_style(lbl_uptime_, &styles::label_secondary, 0);
    lv_obj_set_pos(lbl_uptime_, 470, 0);
    lv_obj_set_width(lbl_uptime_, 280);
    lv_obj_set_style_text_align(lbl_uptime_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_uptime_, "");
    lbl_load_ = lv_label_create(panel);
    lv_obj_add_style(lbl_load_, &styles::label_secondary, 0);
    lv_obj_set_pos(lbl_load_, 470, 23);
    lv_obj_set_width(lbl_load_, 280);
    lv_obj_set_style_text_align(lbl_load_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_load_, "");

    auto make_metric = [&](int x, const char* caption, int range,
                           lv_style_t* indicator, lv_obj_t** bar, lv_obj_t** value) {
        lv_obj_t* tile = lv_obj_create(panel);
        lv_obj_set_size(tile, 244, 94);
        lv_obj_set_pos(tile, x, 50);
        lv_obj_add_style(tile, &styles::metric_tile, 0);
        styles::make_static(tile);

        lv_obj_t* label = lv_label_create(tile);
        lv_obj_add_style(label, &styles::label_secondary, 0);
        lv_obj_set_pos(label, 0, 0);
        lv_label_set_text(label, caption);

        *value = lv_label_create(tile);
        lv_obj_add_style(*value, &styles::label_title, 0);
        lv_obj_set_pos(*value, 0, 21);
        lv_label_set_text(*value, "--");

        *bar = lv_bar_create(tile);
        lv_obj_set_size(*bar, 224, 8);
        lv_obj_set_pos(*bar, 0, 64);
        lv_bar_set_range(*bar, 0, range);
        lv_obj_add_style(*bar, &styles::bar_track, 0);
        lv_obj_add_style(*bar, indicator, LV_PART_INDICATOR);
        lv_obj_clear_flag(*bar, LV_OBJ_FLAG_CLICKABLE);
        styles::make_static(*bar);
    };
    make_metric(0,   "CPU LOAD", 100, &styles::bar_cpu_ind, &bar_cpu_, &lbl_cpu_val_);
    make_metric(254, "MEMORY",   100, &styles::bar_ram_ind, &bar_ram_, &lbl_ram_val_);
    make_metric(508, "THERMAL",  120, &styles::bar_temp_ind, &bar_temp_, &lbl_temp_);

    lv_obj_t* io_panel = lv_obj_create(cont);
    lv_obj_set_width(io_panel, LV_PCT(100));
    lv_obj_set_height(io_panel, LV_SIZE_CONTENT);
    lv_obj_add_style(io_panel, &styles::panel, 0);
    lv_obj_set_flex_flow(io_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(io_panel, 18, 0);
    styles::make_static(io_panel);

    lbl_disks_ = lv_label_create(io_panel);
    lv_obj_add_style(lbl_disks_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_disks_, 370);
    lv_label_set_text(lbl_disks_, "");

    lbl_net_ = lv_label_create(io_panel);
    lv_obj_add_style(lbl_net_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_net_, 370);
    lv_label_set_text(lbl_net_, "");
}

// ── Chart section ─────────────────────────────────────────────────────────────

void ScreenHostDetail::build_chart_section(lv_obj_t* cont) {
    lv_obj_t* lbl = lv_label_create(cont);
    lv_obj_add_style(lbl, &styles::label_value, 0);
    lv_obj_set_style_text_color(lbl, styles::ACCENT, 0);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_text(lbl, "24-HOUR HISTORY");

    auto make_chart = [&](lv_color_t color, const char* title, int ymax) -> lv_obj_t* {
        lv_obj_t* wrapper = lv_obj_create(cont);
        lv_obj_set_width(wrapper, LV_PCT(100));
        lv_obj_set_height(wrapper, 110);
        lv_obj_add_style(wrapper, &styles::panel, 0);
        lv_obj_set_style_pad_all(wrapper, 8, 0);
        styles::make_static(wrapper);

        lv_obj_t* tlbl = lv_label_create(wrapper);
        lv_obj_add_style(tlbl, &styles::label_secondary, 0);
        lv_obj_set_pos(tlbl, 0, 0);
        lv_label_set_text(tlbl, title);

        lv_obj_t* chart = lv_chart_create(wrapper);
        lv_obj_set_size(chart, 760, 80);
        lv_obj_set_pos(chart, 0, 16);
        lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, ymax);
        lv_chart_set_point_count(chart, (uint16_t)HISTORY_LEN);
        lv_obj_add_style(chart, &styles::chart_bg, 0);
        lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
        lv_obj_set_style_line_width(chart, 1, LV_PART_ITEMS);
        lv_obj_set_style_line_color(chart, color, LV_PART_ITEMS);
        lv_chart_set_div_line_count(chart, 3, 0);
        lv_obj_clear_flag(chart, LV_OBJ_FLAG_CLICKABLE);
        styles::make_static(chart);
        return chart;
    };

    chart_cpu_  = make_chart(styles::ACCENT2, "CPU %",  100);
    chart_temp_ = make_chart(styles::WARN,    "Temp C", 120);
    chart_ram_  = make_chart(styles::ACCENT,  "RAM %",  100);

    ser_cpu_  = lv_chart_add_series(chart_cpu_,  styles::ACCENT2, LV_CHART_AXIS_PRIMARY_Y);
    ser_temp_ = lv_chart_add_series(chart_temp_, styles::WARN,    LV_CHART_AXIS_PRIMARY_Y);
    ser_ram_  = lv_chart_add_series(chart_ram_,  styles::ACCENT,  LV_CHART_AXIS_PRIMARY_Y);

    arr_cpu_.fill(LV_CHART_POINT_NONE);
    arr_temp_.fill(LV_CHART_POINT_NONE);
    arr_ram_.fill(LV_CHART_POINT_NONE);

    lv_chart_set_ext_y_array(chart_cpu_,  ser_cpu_,  arr_cpu_.data());
    lv_chart_set_ext_y_array(chart_temp_, ser_temp_, arr_temp_.data());
    lv_chart_set_ext_y_array(chart_ram_,  ser_ram_,  arr_ram_.data());
}

// ── Power section ─────────────────────────────────────────────────────────────

void ScreenHostDetail::build_power_section(lv_obj_t* cont, const HostEntry& e) {
    lv_obj_t* panel = lv_obj_create(cont);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_add_style(panel, &styles::panel, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 7, 0);
    styles::make_static(panel);

    lv_obj_t* lbl = lv_label_create(panel);
    lv_obj_add_style(lbl, &styles::label_value, 0);
    lv_obj_set_style_text_color(lbl, styles::TEXT, 0);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_text(lbl, "QUICK CONTROLS");

    lv_obj_t* ctrl_row = lv_obj_create(panel);
    lv_obj_set_width(ctrl_row, LV_PCT(100));
    lv_obj_set_height(ctrl_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctrl_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(ctrl_row, LV_FLEX_ALIGN_START, 0);
    lv_obj_set_style_pad_column(ctrl_row, 8, 0);
    lv_obj_set_style_bg_opa(ctrl_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctrl_row, 0, 0);
    lv_obj_set_style_pad_all(ctrl_row, 0, 0);
    styles::make_static(ctrl_row);

    auto* d_reboot = new CbData{this, CbData::REBOOT, "", ""};
    cb_data_.emplace_back(d_reboot);
    btn_reboot_ = make_btn(ctrl_row, "Reboot", &styles::btn_action, &styles::btn_action_pressed, d_reboot);

    auto* d_shutdown = new CbData{this, CbData::SHUTDOWN, "", ""};
    cb_data_.emplace_back(d_shutdown);
    btn_shutdown_ = make_btn(ctrl_row, "Shutdown", &styles::btn_danger, nullptr, d_shutdown);

    auto* d_graceful = new CbData{this, CbData::GRACEFUL, "", ""};
    cb_data_.emplace_back(d_graceful);
    btn_graceful_ = make_btn(ctrl_row, "Graceful Off", &styles::btn_danger, nullptr, d_graceful);

    auto* d_wol = new CbData{this, CbData::WOL, "", ""};
    cb_data_.emplace_back(d_wol);
    btn_wol_ = make_btn(ctrl_row, "Wake-on-LAN", &styles::btn_action, &styles::btn_action_pressed, d_wol);
    if (e.online || e.mac.empty())
        lv_obj_add_flag(btn_wol_, LV_OBJ_FLAG_HIDDEN);

    if (e.effective_pdu_outlet() > 0) {
        auto* d_outlet = new CbData{this, CbData::OUTLET, "", ""};
        cb_data_.emplace_back(d_outlet);
        btn_outlet_ = make_btn(ctrl_row, "Outlet Toggle", &styles::btn_action, &styles::btn_action_pressed, d_outlet);
        lbl_outlet_ = lv_label_create(panel);
        lv_obj_add_style(lbl_outlet_, &styles::label_secondary, 0);
        lv_label_set_text(lbl_outlet_, "");
    }

    lbl_shutdown_status_ = lv_label_create(panel);
    lv_obj_add_style(lbl_shutdown_status_, &styles::label_warn, 0);
    lv_obj_set_width(lbl_shutdown_status_, LV_PCT(100));
    lv_label_set_text(lbl_shutdown_status_, "");

    lbl_power_summary_ = lv_label_create(panel);
    lv_obj_add_style(lbl_power_summary_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_power_summary_, LV_PCT(100));
    lv_label_set_text(lbl_power_summary_, "Power metering is available at rack level only.");

    (void)ctrl_row;
}

// ── Service/script section ────────────────────────────────────────────────────

void ScreenHostDetail::build_service_script_section(lv_obj_t* cont, const HostEntry& e) {
    if (e.services.empty() && e.scripts.empty()) return;
    lv_obj_t* panel = lv_obj_create(cont);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_add_style(panel, &styles::panel, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 7, 0);
    styles::make_static(panel);
    cont = panel;

    // Services from agent hello
    bool has_services = false;
    for (auto& svc : e.services) {
        if (!has_services) {
            lv_obj_t* slbl = lv_label_create(cont);
            lv_obj_add_style(slbl, &styles::label_value, 0);
            lv_obj_set_style_text_color(slbl, styles::ACCENT, 0);
            lv_obj_set_width(slbl, LV_PCT(100));
            lv_label_set_text(slbl, "SERVICES");
            has_services = true;
        }
        lv_obj_t* srow = lv_obj_create(cont);
        lv_obj_set_width(srow, LV_PCT(100));
        lv_obj_set_height(srow, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(srow, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(srow, 8, 0);
        lv_obj_set_style_bg_opa(srow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(srow, 0, 0);
        lv_obj_set_style_pad_all(srow, 0, 0);
        styles::make_static(srow);

        lv_obj_t* sname = lv_label_create(srow);
        lv_obj_add_style(sname, &styles::label_secondary, 0);
        lv_obj_set_width(sname, 150);
        lv_label_set_text(sname, svc.name.c_str());

        for (const char* op : {"start", "stop", "restart"}) {
            auto* d = new CbData{this, CbData::SERVICE, svc.name, op};
            cb_data_.emplace_back(d);
            make_btn(srow, op, &styles::btn_action, &styles::btn_action_pressed, d);
        }
        (void)srow;
    }

    // Scripts from agent hello
    bool has_scripts = false;
    for (auto& s : e.scripts) {
        if (!has_scripts) {
            lv_obj_t* slbl = lv_label_create(cont);
            lv_obj_add_style(slbl, &styles::label_value, 0);
            lv_obj_set_style_text_color(slbl, styles::ACCENT, 0);
            lv_obj_set_width(slbl, LV_PCT(100));
            lv_label_set_text(slbl, "SCRIPTS");
            has_scripts = true;
        }
        auto* d = new CbData{this, CbData::SCRIPT, s.name, s.command};
        cb_data_.emplace_back(d);
        make_btn(cont, s.name.c_str(), &styles::btn_action, &styles::btn_action_pressed, d);
    }
}

// ── Machine config section ────────────────────────────────────────────────────

void ScreenHostDetail::build_machine_config_section(lv_obj_t* cont, const HostEntry& e) {
    lv_obj_t* panel = lv_obj_create(cont);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_add_style(panel, &styles::panel, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 7, 0);
    styles::make_static(panel);
    cont = panel;

    lv_obj_t* hdr = lv_label_create(cont);
    lv_obj_add_style(hdr, &styles::label_value, 0);
    lv_obj_set_style_text_color(hdr, styles::ACCENT, 0);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_label_set_text(hdr, "MACHINE CONFIGURATION");

    // Helper: make a row container
    auto make_row = [&]() -> lv_obj_t* {
        lv_obj_t* row = lv_obj_create(cont);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        styles::make_static(row);
        return row;
    };

    // ── Display Name ──
    lv_obj_t* name_row = make_row();

    lv_obj_t* name_key = lv_label_create(name_row);
    lv_obj_add_style(name_key, &styles::label_secondary, 0);
    lv_obj_set_width(name_key, 120);
    lv_label_set_text(name_key, "Name:");

    ta_name_ = lv_textarea_create(name_row);
    lv_obj_set_width(ta_name_, 380);
    lv_obj_set_height(ta_name_, 36);
    lv_textarea_set_one_line(ta_name_, true);
    lv_textarea_set_placeholder_text(ta_name_, e.metrics.host.c_str());
    lv_textarea_set_text(ta_name_, e.display_name.c_str());
    lv_obj_add_style(ta_name_, &styles::label_secondary, 0);

    auto* d_name = new CbData{this, CbData::KB_SHOW_NAME, "", ""};
    cb_data_.emplace_back(d_name);
    lv_obj_add_event_cb(ta_name_, +[](lv_event_t* ev) {
        auto* d = static_cast<ScreenHostDetail::CbData*>(lv_event_get_user_data(ev));
        d->self->show_keyboard(ScreenHostDetail::KB_NAME);
    }, LV_EVENT_CLICKED, d_name);

    auto* d_name_save = new CbData{this, CbData::KB_SHOW_NAME, "", ""};
    cb_data_.emplace_back(d_name_save);
    make_btn(name_row, "Edit", &styles::btn_action, &styles::btn_action_pressed, d_name_save);

    (void)name_row;

    // ── PDU Outlet Override ──
    lv_obj_t* pdu_row = make_row();

    lv_obj_t* pdu_key = lv_label_create(pdu_row);
    lv_obj_add_style(pdu_key, &styles::label_secondary, 0);
    lv_obj_set_width(pdu_key, 120);
    lv_label_set_text(pdu_key, "PDU Outlet:");

    // Show agent's value as context
    char pdu_hint[32];
    snprintf(pdu_hint, sizeof(pdu_hint), "Agent:%d", e.pdu_outlet);
    lv_obj_t* pdu_agent_lbl = lv_label_create(pdu_row);
    lv_obj_add_style(pdu_agent_lbl, &styles::label_secondary, 0);
    lv_obj_set_style_text_color(pdu_agent_lbl, styles::TEXT_DIM, 0);
    lv_label_set_text(pdu_agent_lbl, pdu_hint);

    auto* d_dec = new CbData{this, CbData::PDU_DEC, "", ""};
    cb_data_.emplace_back(d_dec);
    make_btn(pdu_row, "<", &styles::btn_action, &styles::btn_action_pressed, d_dec);

    lbl_pdu_override_val_ = lv_label_create(pdu_row);
    lv_obj_add_style(lbl_pdu_override_val_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_pdu_override_val_, 40);
    lv_obj_set_style_text_align(lbl_pdu_override_val_, LV_TEXT_ALIGN_CENTER, 0);
    update_pdu_override_label();

    auto* d_inc = new CbData{this, CbData::PDU_INC, "", ""};
    cb_data_.emplace_back(d_inc);
    make_btn(pdu_row, ">", &styles::btn_action, &styles::btn_action_pressed, d_inc);

    auto* d_save = new CbData{this, CbData::PDU_SAVE, "", ""};
    cb_data_.emplace_back(d_save);
    make_btn(pdu_row, "Save", &styles::btn_action, &styles::btn_action_pressed, d_save);

    auto* d_clear = new CbData{this, CbData::PDU_CLEAR, "", ""};
    cb_data_.emplace_back(d_clear);
    make_btn(pdu_row, "Clear", &styles::btn_action, &styles::btn_action_pressed, d_clear);

    (void)pdu_row;

    // ── MAC Address ──
    lv_obj_t* mac_row = make_row();

    lv_obj_t* mac_key = lv_label_create(mac_row);
    lv_obj_add_style(mac_key, &styles::label_secondary, 0);
    lv_obj_set_width(mac_key, 120);
    lv_label_set_text(mac_key, "MAC:");

    ta_mac_ = lv_textarea_create(mac_row);
    lv_obj_set_width(ta_mac_, 280);
    lv_obj_set_height(ta_mac_, 36);
    lv_textarea_set_one_line(ta_mac_, true);
    lv_textarea_set_placeholder_text(ta_mac_, "AA:BB:CC:DD:EE:FF");
    lv_textarea_set_text(ta_mac_, e.mac.c_str());
    lv_obj_add_style(ta_mac_, &styles::label_secondary, 0);

    auto* d_mac = new CbData{this, CbData::KB_SHOW_MAC, "", ""};
    cb_data_.emplace_back(d_mac);
    lv_obj_add_event_cb(ta_mac_, +[](lv_event_t* ev) {
        auto* d = static_cast<ScreenHostDetail::CbData*>(lv_event_get_user_data(ev));
        d->self->show_keyboard(ScreenHostDetail::KB_MAC);
    }, LV_EVENT_CLICKED, d_mac);

    auto* d_mac_edit = new CbData{this, CbData::KB_SHOW_MAC, "", ""};
    cb_data_.emplace_back(d_mac_edit);
    make_btn(mac_row, "Edit", &styles::btn_action, &styles::btn_action_pressed, d_mac_edit);

    (void)mac_row;

    // ── Remove Machine ──
    lv_obj_t* remove_row = make_row();

    auto* d_remove = new CbData{this, CbData::REMOVE_HOST, "", ""};
    cb_data_.emplace_back(d_remove);
    make_btn(remove_row, "Remove Machine", &styles::btn_danger, nullptr, d_remove);

    (void)remove_row;

    // Confirmation row (hidden until Remove is clicked)
    confirm_row_ = lv_obj_create(cont);
    lv_obj_set_width(confirm_row_, LV_PCT(100));
    lv_obj_set_height(confirm_row_, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(confirm_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(confirm_row_, 8, 0);
    lv_obj_set_style_bg_opa(confirm_row_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(confirm_row_, 0, 0);
    lv_obj_set_style_pad_all(confirm_row_, 0, 0);
    styles::make_static(confirm_row_);
    lv_obj_add_flag(confirm_row_, LV_OBJ_FLAG_HIDDEN);

    lbl_remove_confirm_ = lv_label_create(confirm_row_);
    lv_obj_add_style(lbl_remove_confirm_, &styles::label_warn, 0);
    lv_obj_set_width(lbl_remove_confirm_, 300);
    lv_label_set_text(lbl_remove_confirm_, "Remove from cache?");

    auto* d_yes = new CbData{this, CbData::REMOVE_CONFIRM, "", ""};
    cb_data_.emplace_back(d_yes);
    make_btn(confirm_row_, "Yes, Remove", &styles::btn_danger, nullptr, d_yes);

    auto* d_no = new CbData{this, CbData::REMOVE_CANCEL, "", ""};
    cb_data_.emplace_back(d_no);
    make_btn(confirm_row_, "Cancel", &styles::btn_action, &styles::btn_action_pressed, d_no);
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void ScreenHostDetail::refresh() {
    auto entry_opt = store_.get(current_host_);
    if (!entry_opt) return;
    auto& e = *entry_opt;
    update_stats(e);
    update_charts(e);
    update_controls(e);
    update_activity();
    poll_graceful_shutdown();
}

void ScreenHostDetail::update_activity() {
    if (!lbl_activity_) return;
    auto recent = activity_.for_host(current_host_, 6);
    if (recent.empty()) {
        lv_label_set_text(lbl_activity_, "No commands have been sent to this host.");
        return;
    }
    std::string text;
    for (const auto& a : recent) {
        const char* state = a.state == ActivityState::Pending ? "..." :
                            a.state == ActivityState::Succeeded ? "OK " : "ERR";
        text += state;
        text += "  " + a.action;
        if (!a.detail.empty()) text += "  " + a.detail;
        if (!a.output.empty()) {
            std::string output = a.output.substr(0, 120);
            for (char& c : output) if (c == '\n' || c == '\r') c = ' ';
            text += "  - " + output;
        }
        text += "\n";
    }
    lv_label_set_text(lbl_activity_, text.c_str());
}

void ScreenHostDetail::update_stats(const HostEntry& e) {
    auto& m = e.metrics;
    char buf[256];

    if (lbl_os_) {
        snprintf(buf, sizeof(buf), "%s  |  %s", e.arch.c_str(), e.os.c_str());
        lv_label_set_text(lbl_os_, buf);
    }

    if (lbl_uptime_ && e.online) {
        long long s = (long long)m.uptime.uptime_s;
        int days = (int)(s / 86400);
        int hours = (int)((s % 86400) / 3600);
        int mins  = (int)((s % 3600) / 60);
        snprintf(buf, sizeof(buf), "Uptime: %dd %dh %dm", days, hours, mins);
        lv_label_set_text(lbl_uptime_, buf);
    }

    if (lbl_load_ && e.online) {
        snprintf(buf, sizeof(buf), "Load: %.2f  %.2f  %.2f",
                 m.uptime.load1, m.uptime.load5, m.uptime.load15);
        lv_label_set_text(lbl_load_, buf);
    }

    if (bar_temp_) {
        float temp = std::isnan(m.cpu_temp_c) ? 0.f : m.cpu_temp_c;
        lv_bar_set_value(bar_temp_, (int32_t)temp, LV_ANIM_OFF);
        if (lbl_temp_) {
            if (std::isnan(m.cpu_temp_c))
                lv_label_set_text(lbl_temp_, "N/A");
            else {
                snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", m.cpu_temp_c);
                lv_label_set_text(lbl_temp_, buf);
                lv_obj_set_style_text_color(lbl_temp_,
                    m.cpu_temp_c > 75.f ? styles::DANGER : styles::TEXT_DIM, 0);
            }
        }
    }

    if (bar_cpu_) lv_bar_set_value(bar_cpu_, (int32_t)m.cpu.usage_pct, LV_ANIM_OFF);
    if (lbl_cpu_val_) {
        snprintf(buf, sizeof(buf), "%.0f%%", m.cpu.usage_pct);
        lv_label_set_text(lbl_cpu_val_, buf);
        lv_obj_set_style_text_color(lbl_cpu_val_,
            m.cpu.usage_pct > 90.f ? styles::DANGER : styles::TEXT_DIM, 0);
    }

    if (bar_ram_) lv_bar_set_value(bar_ram_, (int32_t)m.memory.pct, LV_ANIM_OFF);
    if (lbl_ram_val_) {
        snprintf(buf, sizeof(buf), "%.0f%%  %llu/%llu MB",
                 m.memory.pct,
                 (unsigned long long)(m.memory.used_kb / 1024),
                 (unsigned long long)(m.memory.total_kb / 1024));
        lv_label_set_text(lbl_ram_val_, buf);
    }

    if (lbl_disks_) {
        std::string diskstr;
        for (auto& d : m.disks) {
            char dbuf[80];
            snprintf(dbuf, sizeof(dbuf), "%s  %.0f%%  %lluGB\n",
                     d.path.c_str(), d.pct,
                     (unsigned long long)(d.total_kb / (1024*1024)));
            diskstr += dbuf;
        }
        lv_label_set_text(lbl_disks_, diskstr.c_str());
    }

    if (lbl_net_) {
        std::string netstr;
        for (auto& n : m.net) {
            char nbuf[80];
            snprintf(nbuf, sizeof(nbuf), "%s  rx%.0fKB/s  tx%.0fKB/s\n",
                     n.name.c_str(), n.rx_bps / 1024.f, n.tx_bps / 1024.f);
            netstr += nbuf;
        }
        lv_label_set_text(lbl_net_, netstr.c_str());
    }
}

void ScreenHostDetail::update_charts(const HostEntry& e) {
    if (!chart_cpu_ || !ser_cpu_) return;

    size_t n    = e.history.count;
    size_t head = e.history.head;

    for (size_t i = 0; i < HISTORY_LEN; ++i) {
        if (i < n) {
            size_t src = (head - n + i + HISTORY_LEN) % HISTORY_LEN;
            arr_cpu_[i]  = (lv_coord_t)e.history.buf[src].cpu_pct;
            arr_temp_[i] = (lv_coord_t)e.history.buf[src].cpu_temp;
            arr_ram_[i]  = (lv_coord_t)e.history.buf[src].ram_pct;
        } else {
            arr_cpu_[i] = arr_temp_[i] = arr_ram_[i] = LV_CHART_POINT_NONE;
        }
    }

    lv_chart_set_ext_y_array(chart_cpu_,  ser_cpu_,  arr_cpu_.data());
    lv_chart_set_ext_y_array(chart_temp_, ser_temp_, arr_temp_.data());
    lv_chart_set_ext_y_array(chart_ram_,  ser_ram_,  arr_ram_.data());
    lv_chart_refresh(chart_cpu_);
    lv_chart_refresh(chart_temp_);
    lv_chart_refresh(chart_ram_);
}

void ScreenHostDetail::update_controls(const HostEntry& e) {
    if (btn_wol_) {
        if (!e.online && !e.mac.empty())
            lv_obj_clear_flag(btn_wol_, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(btn_wol_, LV_OBJ_FLAG_HIDDEN);
    }
    if (lbl_outlet_ && e.effective_pdu_outlet() > 0) {
        auto outlet = pdu_.get_outlet(e.effective_pdu_outlet());
        if (outlet) {
            char buf[64];
            snprintf(buf, sizeof(buf), "PDU #%d  %s",
                     outlet->outlet, outlet->on ? "ON" : "OFF");
            lv_label_set_text(lbl_outlet_, buf);
        }
    }
}

void ScreenHostDetail::update_pdu_override_label() {
    if (!lbl_pdu_override_val_) return;
    char buf[16];
    if (pdu_override_pending_ < 0)
        snprintf(buf, sizeof(buf), "--");
    else
        snprintf(buf, sizeof(buf), "%d", pdu_override_pending_);
    lv_label_set_text(lbl_pdu_override_val_, buf);
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void ScreenHostDetail::show_keyboard(KbTarget target) {
    if (!kb_) return;
    kb_target_ = target;
    lv_keyboard_set_textarea(kb_, target == KB_NAME ? ta_name_ : ta_mac_);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb_);
}

void ScreenHostDetail::hide_keyboard(bool save) {
    if (!kb_) return;
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    if (save) {
        if (kb_target_ == KB_NAME)      save_display_name();
        else if (kb_target_ == KB_MAC)  save_mac();
    }
    kb_target_ = KB_NONE;
}

void ScreenHostDetail::save_display_name() {
    if (!ta_name_) return;
    const char* text = lv_textarea_get_text(ta_name_);
    store_.set_display_name(current_host_, text ? text : "");
}

void ScreenHostDetail::save_mac() {
    if (!ta_mac_) return;
    const char* text = lv_textarea_get_text(ta_mac_);
    store_.set_mac(current_host_, text ? text : "");
}

// ── CRUD actions ──────────────────────────────────────────────────────────────

void ScreenHostDetail::on_pdu_dec() {
    if (pdu_override_pending_ <= 0) return;
    --pdu_override_pending_;
    update_pdu_override_label();
}

void ScreenHostDetail::on_pdu_inc() {
    if (pdu_override_pending_ >= 8) return;
    if (pdu_override_pending_ < 0) pdu_override_pending_ = 0;
    ++pdu_override_pending_;
    update_pdu_override_label();
}

void ScreenHostDetail::on_pdu_save() {
    store_.set_pdu_outlet_override(current_host_, pdu_override_pending_);
    // Force a full rebuild so the outlet toggle button reflects the new effective outlet
    std::string host = current_host_;
    current_host_.clear();
    show(host);
}

void ScreenHostDetail::on_pdu_clear() {
    pdu_override_pending_ = -1;
    store_.set_pdu_outlet_override(current_host_, -1);
    update_pdu_override_label();
}

void ScreenHostDetail::on_remove_host() {
    if (confirm_row_) lv_obj_clear_flag(confirm_row_, LV_OBJ_FLAG_HIDDEN);
}

void ScreenHostDetail::on_remove_confirm() {
    store_.remove_host(current_host_);
    current_host_.clear();
    hide();
    on_back_();
}

void ScreenHostDetail::on_remove_cancel() {
    if (confirm_row_) lv_obj_add_flag(confirm_row_, LV_OBJ_FLAG_HIDDEN);
}

// ── Action handlers ───────────────────────────────────────────────────────────

void ScreenHostDetail::on_reboot() {
    auto e = store_.get(current_host_);
    if (!e || !e->online) return;
    protocol::CommandMessage cmd;
    cmd.id = current_host_ + "_reboot";
    cmd.action = "reboot";
    router_.dispatch(cmd, current_host_, e->effective_pdu_outlet());
}

void ScreenHostDetail::on_shutdown() {
    auto e = store_.get(current_host_);
    if (!e || !e->online) return;
    protocol::CommandMessage cmd;
    cmd.id = current_host_ + "_shutdown";
    cmd.action = "shutdown";
    router_.dispatch(cmd, current_host_, e->effective_pdu_outlet());
}

void ScreenHostDetail::on_shutdown_graceful() {
    auto e = store_.get(current_host_);
    if (!e || !e->online || e->effective_pdu_outlet() <= 0) {
        on_shutdown();
        return;
    }
    shutdown_state_   = PowerState::ShutdownPending;
    shutdown_started_ = std::chrono::steady_clock::now();

    protocol::CommandMessage cmd;
    cmd.id = current_host_ + "_graceful";
    cmd.action = "shutdown";
    router_.dispatch(cmd, current_host_, e->effective_pdu_outlet());

    if (lbl_shutdown_status_)
        lv_label_set_text(lbl_shutdown_status_, "Shutdown sent -- waiting for offline...");
    if (btn_graceful_) lv_obj_clear_flag(btn_graceful_, LV_OBJ_FLAG_CLICKABLE);
    if (btn_shutdown_) lv_obj_clear_flag(btn_shutdown_, LV_OBJ_FLAG_CLICKABLE);

    if (!shutdown_poll_timer_) {
        shutdown_poll_timer_ = lv_timer_create(+[](lv_timer_t* t) {
            auto* self = static_cast<ScreenHostDetail*>(lv_timer_get_user_data(t));
            self->poll_graceful_shutdown();
        }, 2000, this);
    }
}

void ScreenHostDetail::on_wol() {
    auto e = store_.get(current_host_);
    if (!e || e->mac.empty()) return;
    router_.dispatch_wol(current_host_, e->mac);
}

void ScreenHostDetail::on_outlet_toggle() {
    auto e = store_.get(current_host_);
    if (!e || e->effective_pdu_outlet() <= 0) return;
    auto outlet = pdu_.get_outlet(e->effective_pdu_outlet());
    bool cur = outlet ? outlet->on : false;
    protocol::CommandMessage cmd;
    cmd.id = current_host_ + "_outlet";
    cmd.action = "outlet";
    cmd.outlet_num = e->effective_pdu_outlet();
    cmd.outlet_state = !cur;
    router_.dispatch(cmd, current_host_, e->effective_pdu_outlet());
}

void ScreenHostDetail::on_service(const std::string& svc, const std::string& op) {
    auto e = store_.get(current_host_);
    if (!e || !e->online) return;
    protocol::CommandMessage cmd;
    cmd.id = current_host_ + "_svc_" + svc;
    cmd.action = "service";
    cmd.service_name = svc;
    cmd.service_op   = op;
    router_.dispatch(cmd, current_host_, e->effective_pdu_outlet());
}

void ScreenHostDetail::on_script(const std::string& name, const std::string& command) {
    auto e = store_.get(current_host_);
    if (!e || !e->online) return;
    protocol::CommandMessage cmd;
    cmd.id = current_host_ + "_script_" + name;
    cmd.action = "script";
    cmd.service_name = command;
    cmd.service_op = name;
    router_.dispatch(cmd, current_host_, e->effective_pdu_outlet());
}

// ── Graceful shutdown poll ────────────────────────────────────────────────────

void ScreenHostDetail::poll_graceful_shutdown() {
    if (shutdown_state_ == PowerState::Normal) return;

    auto entry = store_.get(current_host_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - shutdown_started_).count();

    if (shutdown_state_ == PowerState::ShutdownPending) {
        bool still_online = entry && entry->online;
        if (!still_online) {
            shutdown_state_ = PowerState::WaitingOutlet;
            if (lbl_shutdown_status_)
                lv_label_set_text(lbl_shutdown_status_, "Offline -- disabling assigned outlet...");
        } else if (elapsed > 120) {
            if (lbl_shutdown_status_)
                lv_label_set_text(lbl_shutdown_status_, "WARN: shutdown timeout -- host still online");
            reset_shutdown_state();
        }
        return;
    }

    if (shutdown_state_ == PowerState::WaitingOutlet) {
        if (entry && entry->effective_pdu_outlet() > 0) {
            auto outlet = pdu_.get_outlet(entry->effective_pdu_outlet());
            if (outlet) {
                shutdown_state_ = PowerState::OutletOff;
                protocol::CommandMessage cmd;
                cmd.id = current_host_ + "_outlet_off";
                cmd.action = "outlet";
                cmd.outlet_num = entry->effective_pdu_outlet();
                cmd.outlet_state = false;
                router_.dispatch(cmd, current_host_, entry->effective_pdu_outlet());
                if (lbl_shutdown_status_)
                    lv_label_set_text(lbl_shutdown_status_, "Outlet off. Graceful shutdown complete. OK");
                lv_timer_t* t = lv_timer_create(+[](lv_timer_t* t) {
                    auto* self = static_cast<ScreenHostDetail*>(lv_timer_get_user_data(t));
                    self->reset_shutdown_state();
                    lv_timer_del(t);
                }, 3000, this);
                (void)t;
                if (shutdown_poll_timer_) {
                    lv_timer_del(shutdown_poll_timer_);
                    shutdown_poll_timer_ = nullptr;
                }
                return;
            }
        }
        if (elapsed > 300) {
            if (lbl_shutdown_status_)
                lv_label_set_text(lbl_shutdown_status_, "WARN: unable to disable assigned outlet");
            reset_shutdown_state();
        }
    }
}

void ScreenHostDetail::reset_shutdown_state() {
    shutdown_state_ = PowerState::Normal;
    if (shutdown_poll_timer_) {
        lv_timer_del(shutdown_poll_timer_);
        shutdown_poll_timer_ = nullptr;
    }
    if (lbl_shutdown_status_) lv_label_set_text(lbl_shutdown_status_, "");
    if (btn_graceful_) lv_obj_add_flag(btn_graceful_, LV_OBJ_FLAG_CLICKABLE);
    if (btn_shutdown_) lv_obj_add_flag(btn_shutdown_, LV_OBJ_FLAG_CLICKABLE);
}
