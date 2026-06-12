#include "screen_overview.h"
#include "styles.h"
#include <cmath>
#include <cstdio>

enum RowIdx {
    RI_STRIP = 0, RI_HOST, RI_STATUS, RI_CPU_BAR, RI_CPU,
    RI_RAM_BAR, RI_RAM, RI_TEMP, RI_WATTS
};

struct RowTapData { std::string host; ScreenOverview::TapCb cb; };

static lv_obj_t* find_row(lv_obj_t* list, const std::string& host) {
    uint32_t count = lv_obj_get_child_count(list);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* row = lv_obj_get_child(list, i);
        auto* name = static_cast<std::string*>(lv_obj_get_user_data(row));
        if (name && *name == host) return row;
    }
    return nullptr;
}

static lv_obj_t* make_stat(lv_obj_t* parent, const char* label,
                           lv_color_t color, lv_obj_t** value) {
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_size(tile, 188, 72);
    lv_obj_add_style(tile, &styles::metric_tile, 0);
    styles::make_static(tile);

    lv_obj_t* accent = lv_obj_create(tile);
    lv_obj_set_size(accent, 3, 44);
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, -8, 0);
    lv_obj_set_style_bg_color(accent, color, 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 2, 0);
    styles::make_static(accent);

    *value = lv_label_create(tile);
    lv_obj_add_style(*value, &styles::label_title, 0);
    lv_obj_set_style_text_color(*value, color, 0);
    lv_obj_align(*value, LV_ALIGN_LEFT_MID, 6, -8);

    lv_obj_t* caption = lv_label_create(tile);
    lv_obj_add_style(caption, &styles::label_secondary, 0);
    lv_obj_set_style_text_color(caption, styles::TEXT_DIM, 0);
    lv_obj_align(caption, LV_ALIGN_LEFT_MID, 6, 15);
    lv_label_set_text(caption, label);
    return tile;
}

static lv_obj_t* create_node_row(lv_obj_t* parent, const std::string& host,
                                 ScreenOverview::TapCb on_tap) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 62);
    lv_obj_set_style_bg_color(row, styles::BG_RAISED, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, styles::BORDER, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    styles::make_static(row);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(row, new std::string(host));

    if (on_tap) {
        auto* data = new RowTapData{host, std::move(on_tap)};
        lv_obj_add_event_cb(row, +[](lv_event_t* e) {
            auto* d = static_cast<RowTapData*>(lv_event_get_user_data(e));
            if (d->cb) d->cb(d->host);
        }, LV_EVENT_CLICKED, data);
    }

    lv_obj_t* strip = lv_obj_create(row);
    lv_obj_set_size(strip, 4, 62);
    lv_obj_set_pos(strip, 0, 0);
    lv_obj_add_style(strip, &styles::status_strip_offline, 0);
    lv_obj_set_style_radius(strip, 6, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    styles::make_static(strip);

    lv_obj_t* name = lv_label_create(row);
    lv_obj_add_style(name, &styles::label_value, 0);
    lv_obj_set_pos(name, 13, 9);
    lv_obj_set_width(name, 142);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

    lv_obj_t* status = lv_label_create(row);
    lv_obj_add_style(status, &styles::label_secondary, 0);
    lv_obj_set_pos(status, 13, 34);
    lv_obj_set_width(status, 142);

    auto make_bar = [&](int y, lv_style_t* indicator) {
        lv_obj_t* bar = lv_bar_create(row);
        lv_obj_set_size(bar, 150, 7);
        lv_obj_set_pos(bar, 166, y);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_add_style(bar, &styles::bar_track, 0);
        lv_obj_add_style(bar, indicator, LV_PART_INDICATOR);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        styles::make_static(bar);
        return bar;
    };
    make_bar(15, &styles::bar_cpu_ind);

    lv_obj_t* cpu = lv_label_create(row);
    lv_obj_add_style(cpu, &styles::label_secondary, 0);
    lv_obj_set_pos(cpu, 324, 8);
    lv_obj_set_width(cpu, 42);

    make_bar(40, &styles::bar_ram_ind);

    lv_obj_t* ram = lv_label_create(row);
    lv_obj_add_style(ram, &styles::label_secondary, 0);
    lv_obj_set_pos(ram, 324, 33);
    lv_obj_set_width(ram, 42);

    lv_obj_t* temp = lv_label_create(row);
    lv_obj_add_style(temp, &styles::label_value, 0);
    lv_obj_set_pos(temp, 377, 9);
    lv_obj_set_width(temp, 70);

    lv_obj_t* watts = lv_label_create(row);
    lv_obj_add_style(watts, &styles::label_secondary, 0);
    lv_obj_set_pos(watts, 370, 34);
    lv_obj_set_width(watts, 108);
    styles::make_scroll_passthrough(row);
    return row;
}

ScreenOverview::ScreenOverview(lv_obj_t* parent, TapCb on_tap)
    : on_tap_(std::move(on_tap)) {
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, 800, 390);
    lv_obj_set_style_bg_color(container_, styles::BG, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 8, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    styles::make_static(container_);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);

    build_stats_panel(container_);
    build_alert_bar(container_);
    build_host_list(container_);
}

void ScreenOverview::build_stats_panel(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 784, 78);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    styles::make_static(panel);

    make_stat(panel, "NODES ONLINE", styles::OK, &lbl_online_count_);
    make_stat(panel, "AVERAGE CPU", styles::ACCENT2, &lbl_avg_cpu_);
    make_stat(panel, "AVERAGE MEMORY", styles::ACCENT, &lbl_avg_ram_);
    make_stat(panel, "RACK POWER", styles::WARN, &lbl_watts_);
}

void ScreenOverview::build_alert_bar(lv_obj_t* parent) {
    alert_bar_ = lv_obj_create(parent);
    lv_obj_set_size(alert_bar_, 270, 288);
    lv_obj_set_pos(alert_bar_, 0, 86);
    lv_obj_add_style(alert_bar_, &styles::panel, 0);
    styles::make_static(alert_bar_);

    lv_obj_t* title = lv_label_create(alert_bar_);
    lv_obj_add_style(title, &styles::label_value, 0);
    lv_obj_set_style_text_color(title, styles::TEXT, 0);
    lv_obj_set_pos(title, 2, 0);
    lv_label_set_text(title, "ATTENTION");

    lv_obj_t* hint = lv_label_create(alert_bar_);
    lv_obj_add_style(hint, &styles::label_secondary, 0);
    lv_obj_set_pos(hint, 2, 23);
    lv_label_set_text(hint, "Current rack health signals");

    lbl_alert_ = lv_label_create(alert_bar_);
    lv_obj_add_style(lbl_alert_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_alert_, 246);
    lv_obj_set_pos(lbl_alert_, 2, 58);
    lv_obj_set_style_text_line_space(lbl_alert_, 8, 0);
    lv_label_set_text(lbl_alert_, "No active alerts.\nRack is operating normally.");
}

void ScreenOverview::build_host_list(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 506, 288);
    lv_obj_set_pos(panel, 278, 86);
    lv_obj_add_style(panel, &styles::panel, 0);
    styles::make_static(panel);

    lv_obj_t* title = lv_label_create(panel);
    lv_obj_add_style(title, &styles::label_value, 0);
    lv_obj_set_style_text_color(title, styles::TEXT, 0);
    lv_obj_set_pos(title, 2, 0);
    lv_label_set_text(title, "RACK NODES");

    lv_obj_t* hint = lv_label_create(panel);
    lv_obj_add_style(hint, &styles::label_secondary, 0);
    lv_obj_set_pos(hint, 2, 23);
    lv_label_set_text(hint, "Tap a node for controls and history");

    list_ = lv_obj_create(panel);
    lv_obj_set_size(list_, 486, 226);
    lv_obj_set_pos(list_, 0, 42);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_, 0, 0);
    lv_obj_set_style_pad_all(list_, 0, 0);
    lv_obj_set_style_pad_row(list_, 6, 0);
    styles::make_vertical_scroll(list_);
}

void ScreenOverview::refresh(const std::map<std::string, HostEntry>& hosts,
                             const std::vector<protocol::OutletState>& outlets,
                             bool pdu_enabled, bool power_warning, bool power_critical) {
    int online = 0;
    float cpu = 0, ram = 0, watts = 0;
    for (const auto& [name, e] : hosts) {
        (void)name;
        if (!e.online) continue;
        ++online;
        cpu += e.metrics.cpu.usage_pct;
        ram += e.metrics.memory.pct;
    }
    if (online) { cpu /= online; ram /= online; }
    for (const auto& outlet : outlets) watts += outlet.watts;

    char buf[64];
    snprintf(buf, sizeof(buf), "%d / %zu", online, hosts.size());
    lv_label_set_text(lbl_online_count_, buf);
    lv_obj_set_style_text_color(lbl_online_count_,
        online == (int)hosts.size() ? styles::OK : styles::WARN, 0);
    snprintf(buf, sizeof(buf), "%.0f%%", cpu);
    lv_label_set_text(lbl_avg_cpu_, buf);
    snprintf(buf, sizeof(buf), "%.0f%%", ram);
    lv_label_set_text(lbl_avg_ram_, buf);
    if (pdu_enabled) snprintf(buf, sizeof(buf), "%.0f W", watts);
    else snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(lbl_watts_, buf);

    std::string alerts;
    int alert_count = 0;
    int displayed_alerts = 0;
    if (power_warning) {
        ++alert_count;
        alerts += power_critical ? LV_SYMBOL_WARNING "  RACK POWER CRITICAL\n    SHEDDING MAY START\n"
                                 : LV_SYMBOL_WARNING "  RACK POWER WARNING\n    BUDGET THRESHOLD REACHED\n";
        ++displayed_alerts;
    }
    for (const auto& [name, e] : hosts) {
        const std::string& label = e.display_name.empty() ? name : e.display_name;
        if (!e.online) {
            ++alert_count;
            if (displayed_alerts < 4) {
                alerts += LV_SYMBOL_CLOSE "  " + label + "\n    OFFLINE\n";
                ++displayed_alerts;
            }
        } else if ((!std::isnan(e.metrics.cpu_temp_c) && e.metrics.cpu_temp_c > 75.f)
                   || e.metrics.cpu.usage_pct > 90.f) {
            ++alert_count;
            if (displayed_alerts < 4) {
                char line[120];
                snprintf(line, sizeof(line), LV_SYMBOL_WARNING "  %s\n    CPU %.0f%%  TEMP %.0fC\n",
                         label.c_str(), e.metrics.cpu.usage_pct, e.metrics.cpu_temp_c);
                alerts += line;
                ++displayed_alerts;
            }
        }
    }
    if (alert_count > displayed_alerts)
        alerts += "\n+" + std::to_string(alert_count - displayed_alerts) + " more";
    if (hosts.empty()) {
        alerts = "No agents connected.\n\nInstall deck-agent on a node\nto begin monitoring.";
        lv_obj_set_style_text_color(lbl_alert_, styles::TEXT_DIM, 0);
    } else if (alerts.empty()) {
        alerts = LV_SYMBOL_OK "  ALL SYSTEMS NOMINAL\n\nNo offline, hot, or overloaded\nnodes detected.";
        lv_obj_set_style_text_color(lbl_alert_, styles::OK, 0);
    } else {
        lv_obj_set_style_text_color(lbl_alert_, styles::WARN, 0);
    }
    lv_label_set_text(lbl_alert_, alerts.c_str());

    uint32_t count = lv_obj_get_child_count(list_);
    for (int32_t i = (int32_t)count - 1; i >= 0; --i) {
        lv_obj_t* row = lv_obj_get_child(list_, i);
        auto* name = static_cast<std::string*>(lv_obj_get_user_data(row));
        if (name && hosts.find(*name) == hosts.end()) lv_obj_del(row);
    }

    for (const auto& [name, e] : hosts) {
        lv_obj_t* row = find_row(list_, name);
        if (!row) row = create_node_row(list_, name, on_tap_);
        const std::string& label = e.display_name.empty() ? name : e.display_name;
        lv_label_set_text(lv_obj_get_child(row, RI_HOST), label.c_str());
        lv_label_set_text(lv_obj_get_child(row, RI_STATUS), e.online ? "ONLINE" : "OFFLINE");
        lv_obj_set_style_text_color(lv_obj_get_child(row, RI_STATUS),
            e.online ? styles::OK : styles::TEXT_DIM, 0);

        lv_obj_t* strip = lv_obj_get_child(row, RI_STRIP);
        lv_obj_remove_style(strip, &styles::status_strip_ok, 0);
        lv_obj_remove_style(strip, &styles::status_strip_warn, 0);
        lv_obj_remove_style(strip, &styles::status_strip_danger, 0);
        lv_obj_remove_style(strip, &styles::status_strip_offline, 0);
        bool danger = e.online && ((!std::isnan(e.metrics.cpu_temp_c) && e.metrics.cpu_temp_c > 75.f)
                                   || e.metrics.cpu.usage_pct > 90.f);
        lv_obj_add_style(strip, !e.online ? &styles::status_strip_offline :
                                danger ? &styles::status_strip_danger :
                                         &styles::status_strip_ok, 0);

        lv_bar_set_value(lv_obj_get_child(row, RI_CPU_BAR),
                         (int)e.metrics.cpu.usage_pct, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "%.0f%%", e.metrics.cpu.usage_pct);
        lv_label_set_text(lv_obj_get_child(row, RI_CPU), buf);
        lv_bar_set_value(lv_obj_get_child(row, RI_RAM_BAR),
                         (int)e.metrics.memory.pct, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "%.0f%%", e.metrics.memory.pct);
        lv_label_set_text(lv_obj_get_child(row, RI_RAM), buf);

        lv_obj_t* temp = lv_obj_get_child(row, RI_TEMP);
        if (e.online && !std::isnan(e.metrics.cpu_temp_c)) {
            snprintf(buf, sizeof(buf), "%.0fC", e.metrics.cpu_temp_c);
            lv_label_set_text(temp, buf);
            lv_obj_set_style_text_color(temp,
                e.metrics.cpu_temp_c > 75.f ? styles::DANGER : styles::TEXT, 0);
        } else lv_label_set_text(temp, "--");

        lv_obj_t* power = lv_obj_get_child(row, RI_WATTS);
        lv_label_set_text(power, "");
        if (pdu_enabled && e.effective_pdu_outlet() > 0) {
            for (const auto& outlet : outlets) {
                if (outlet.outlet != e.effective_pdu_outlet()) continue;
                snprintf(buf, sizeof(buf), "PDU %d  %.0fW", outlet.outlet, outlet.watts);
                lv_label_set_text(power, buf);
                break;
            }
        }
        lv_obj_set_style_opa(row, e.online ? LV_OPA_COVER : LV_OPA_60, 0);
    }
}
