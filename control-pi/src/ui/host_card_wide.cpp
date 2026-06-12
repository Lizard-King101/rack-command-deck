#include "host_card_wide.h"
#include "styles.h"
#include <cstdio>
#include <cmath>
#include <memory>
#include <vector>

static constexpr int CARD_H   = 96;
static constexpr int STRIP_W  = 4;
static constexpr int PAD_L    = 10;  // left padding after strip
static constexpr int BAR_W    = 420;
static constexpr int BAR_H    = 10;

// Child indices — must match create_card order
enum WIdx {
    WIDX_STRIP = 0,
    WIDX_HOSTNAME,
    WIDX_ARCH_BADGE,
    WIDX_OS,
    WIDX_CPU_LBL,
    WIDX_BAR_CPU,
    WIDX_CPU_VAL,
    WIDX_RAM_LBL,
    WIDX_BAR_RAM,
    WIDX_RAM_VAL,
    WIDX_BOTTOM,   // single label: temp | uptime | wattage
    WIDX_COUNT
};

struct TapData {
    std::string host;
    HostCardWide::TapCb cb;
};

lv_obj_t* HostCardWide::find_card(lv_obj_t* parent, const std::string& host) {
    uint32_t n = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* c = lv_obj_get_child(parent, i);
        auto* s = static_cast<std::string*>(lv_obj_get_user_data(c));
        if (s && *s == host) return c;
    }
    return nullptr;
}

lv_obj_t* HostCardWide::create_card(lv_obj_t* parent, const std::string& host, TapCb on_tap) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, CARD_H);
    lv_obj_add_style(card, &styles::card_wide, 0);
    lv_obj_add_style(card, &styles::card_wide_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(card, 0, 0);
    styles::make_static(card);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    // Store hostname as user_data for find_card
    lv_obj_set_user_data(card, new std::string(host));

    // Wire tap callback
    if (on_tap) {
        auto* td = new TapData{ host, std::move(on_tap) };
        lv_obj_add_event_cb(card, +[](lv_event_t* e) {
            auto* d = static_cast<TapData*>(lv_event_get_user_data(e));
            if (d && d->cb) d->cb(d->host);
        }, LV_EVENT_CLICKED, td);
        // Note: td leaks when card is deleted — acceptable for long-lived UI objects
    }

    // WIDX_STRIP — left status bar
    lv_obj_t* strip = lv_obj_create(card);
    lv_obj_set_size(strip, STRIP_W, CARD_H);
    lv_obj_set_pos(strip, 0, 0);
    lv_obj_add_style(strip, &styles::status_strip_offline, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    styles::make_static(strip);

    int x = STRIP_W + PAD_L;

    // WIDX_HOSTNAME
    lv_obj_t* l_host = lv_label_create(card);
    lv_obj_add_style(l_host, &styles::label_title, 0);
    lv_obj_set_pos(l_host, x, 8);
    lv_obj_set_width(l_host, 280);
    lv_label_set_long_mode(l_host, LV_LABEL_LONG_DOT);
    lv_label_set_text(l_host, host.c_str());

    // WIDX_ARCH_BADGE
    lv_obj_t* l_arch = lv_label_create(card);
    lv_obj_add_style(l_arch, &styles::label_badge, 0);
    lv_obj_set_pos(l_arch, x + 290, 10);
    lv_label_set_text(l_arch, "");

    // WIDX_OS
    lv_obj_t* l_os = lv_label_create(card);
    lv_obj_add_style(l_os, &styles::label_secondary, 0);
    lv_obj_set_width(l_os, 200);
    lv_obj_align(l_os, LV_ALIGN_TOP_RIGHT, -8, 10);
    lv_label_set_long_mode(l_os, LV_LABEL_LONG_DOT);
    lv_label_set_text(l_os, "");

    // WIDX_CPU_LBL
    lv_obj_t* l_cpu_lbl = lv_label_create(card);
    lv_obj_add_style(l_cpu_lbl, &styles::label_secondary, 0);
    lv_obj_set_pos(l_cpu_lbl, x, 36);
    lv_label_set_text(l_cpu_lbl, "CPU");

    // WIDX_BAR_CPU
    lv_obj_t* bar_cpu = lv_bar_create(card);
    lv_obj_set_size(bar_cpu, BAR_W, BAR_H);
    lv_obj_set_pos(bar_cpu, x + 34, 38);
    lv_bar_set_range(bar_cpu, 0, 100);
    lv_obj_add_style(bar_cpu, &styles::bar_track, 0);
    lv_obj_add_style(bar_cpu, &styles::bar_cpu_ind, LV_PART_INDICATOR);
    lv_obj_clear_flag(bar_cpu, LV_OBJ_FLAG_CLICKABLE);
    styles::make_static(bar_cpu);

    // WIDX_CPU_VAL
    lv_obj_t* l_cpu_val = lv_label_create(card);
    lv_obj_add_style(l_cpu_val, &styles::label_secondary, 0);
    lv_obj_set_pos(l_cpu_val, x + 34 + BAR_W + 6, 34);
    lv_label_set_text(l_cpu_val, "");

    // WIDX_RAM_LBL
    lv_obj_t* l_ram_lbl = lv_label_create(card);
    lv_obj_add_style(l_ram_lbl, &styles::label_secondary, 0);
    lv_obj_set_pos(l_ram_lbl, x, 54);
    lv_label_set_text(l_ram_lbl, "RAM");

    // WIDX_BAR_RAM
    lv_obj_t* bar_ram = lv_bar_create(card);
    lv_obj_set_size(bar_ram, BAR_W, BAR_H);
    lv_obj_set_pos(bar_ram, x + 34, 56);
    lv_bar_set_range(bar_ram, 0, 100);
    lv_obj_add_style(bar_ram, &styles::bar_track, 0);
    lv_obj_add_style(bar_ram, &styles::bar_ram_ind, LV_PART_INDICATOR);
    lv_obj_clear_flag(bar_ram, LV_OBJ_FLAG_CLICKABLE);
    styles::make_static(bar_ram);

    // WIDX_RAM_VAL
    lv_obj_t* l_ram_val = lv_label_create(card);
    lv_obj_add_style(l_ram_val, &styles::label_secondary, 0);
    lv_obj_set_pos(l_ram_val, x + 34 + BAR_W + 6, 52);
    lv_label_set_text(l_ram_val, "");

    // WIDX_BOTTOM — single dim status line
    lv_obj_t* l_bottom = lv_label_create(card);
    lv_obj_add_style(l_bottom, &styles::label_secondary, 0);
    lv_obj_set_pos(l_bottom, x, 74);
    lv_obj_set_width(l_bottom, 750);
    lv_label_set_long_mode(l_bottom, LV_LABEL_LONG_DOT);
    lv_label_set_text(l_bottom, "");

    (void)strip; (void)l_host; (void)l_arch; (void)l_os;
    (void)l_cpu_lbl; (void)bar_cpu; (void)l_cpu_val;
    (void)l_ram_lbl; (void)bar_ram; (void)l_ram_val;
    (void)l_bottom;

    styles::make_scroll_passthrough(card);
    return card;
}

void HostCardWide::populate(lv_obj_t* card, const HostEntry& e,
                             std::optional<protocol::OutletState> outlet,
                             bool pdu_enabled) {
    auto& m = e.metrics;
    char buf[128];

    lv_opa_t dim = e.online ? LV_OPA_COVER : LV_OPA_40;

    // Determine strip color from health
    lv_obj_t* strip = lv_obj_get_child(card, WIDX_STRIP);
    if (!e.online) {
        lv_obj_remove_style(strip, &styles::status_strip_ok, 0);
        lv_obj_remove_style(strip, &styles::status_strip_warn, 0);
        lv_obj_remove_style(strip, &styles::status_strip_danger, 0);
        lv_obj_add_style(strip, &styles::status_strip_offline, 0);
    } else {
        bool hot    = !std::isnan(m.cpu_temp_c) && m.cpu_temp_c > 75.f;
        bool loaded = m.cpu.usage_pct > 90.f;
        bool warn   = !std::isnan(m.cpu_temp_c) && m.cpu_temp_c > 65.f;
        lv_obj_remove_style(strip, &styles::status_strip_offline, 0);
        lv_obj_remove_style(strip, &styles::status_strip_ok, 0);
        lv_obj_remove_style(strip, &styles::status_strip_warn, 0);
        lv_obj_remove_style(strip, &styles::status_strip_danger, 0);
        if (hot || loaded)
            lv_obj_add_style(strip, &styles::status_strip_danger, 0);
        else if (warn)
            lv_obj_add_style(strip, &styles::status_strip_warn, 0);
        else
            lv_obj_add_style(strip, &styles::status_strip_ok, 0);
    }
    lv_obj_refresh_style(strip, LV_PART_MAIN, LV_STYLE_PROP_ANY);

    // Hostname (show display_name if set)
    lv_obj_t* l_host = lv_obj_get_child(card, WIDX_HOSTNAME);
    const std::string& disp = e.display_name.empty() ? m.host : e.display_name;
    lv_label_set_text(l_host, disp.empty() ? "unknown" : disp.c_str());
    lv_obj_set_style_opa(l_host, LV_OPA_COVER, 0);

    // Arch badge
    lv_obj_t* l_arch = lv_obj_get_child(card, WIDX_ARCH_BADGE);
    lv_label_set_text(l_arch, e.arch.empty() ? "" : e.arch.c_str());
    lv_obj_set_style_opa(l_arch, dim, 0);

    // OS
    lv_obj_t* l_os = lv_obj_get_child(card, WIDX_OS);
    lv_label_set_text(l_os, e.os.empty() ? "" : e.os.c_str());
    lv_obj_set_style_opa(l_os, dim, 0);

    // CPU bar + value
    lv_obj_t* bar_cpu = lv_obj_get_child(card, WIDX_BAR_CPU);
    lv_bar_set_value(bar_cpu, (int32_t)m.cpu.usage_pct, LV_ANIM_OFF);
    lv_obj_set_style_opa(bar_cpu, e.online ? LV_OPA_COVER : LV_OPA_30, 0);

    lv_obj_t* l_cpu_val = lv_obj_get_child(card, WIDX_CPU_VAL);
    if (!std::isnan(m.cpu_temp_c) && m.cpu_temp_c > 0)
        snprintf(buf, sizeof(buf), "%.0f%%  %.1f\xC2\xB0""C", m.cpu.usage_pct, m.cpu_temp_c);
    else
        snprintf(buf, sizeof(buf), "%.0f%%", m.cpu.usage_pct);
    lv_label_set_text(l_cpu_val, buf);
    bool hot = !std::isnan(m.cpu_temp_c) && m.cpu_temp_c > 75.f;
    lv_obj_set_style_text_color(l_cpu_val, hot ? styles::DANGER : styles::TEXT_DIM, 0);
    lv_obj_set_style_opa(l_cpu_val, dim, 0);

    // RAM bar + value
    lv_obj_t* bar_ram = lv_obj_get_child(card, WIDX_BAR_RAM);
    lv_bar_set_value(bar_ram, (int32_t)m.memory.pct, LV_ANIM_OFF);
    lv_obj_set_style_opa(bar_ram, e.online ? LV_OPA_COVER : LV_OPA_30, 0);

    lv_obj_t* l_ram_val = lv_obj_get_child(card, WIDX_RAM_VAL);
    snprintf(buf, sizeof(buf), "%.0f%%  %llu/%llu MB",
             m.memory.pct,
             (unsigned long long)(m.memory.used_kb / 1024),
             (unsigned long long)(m.memory.total_kb / 1024));
    lv_label_set_text(l_ram_val, buf);
    lv_obj_set_style_opa(l_ram_val, dim, 0);

    // Bottom row: load, uptime, optional wattage
    lv_obj_t* l_bottom = lv_obj_get_child(card, WIDX_BOTTOM);
    if (e.online) {
        if (pdu_enabled && outlet) {
            snprintf(buf, sizeof(buf), "load %.2f  up %.0fh  |  PDU #%d %.1fW %s",
                     m.uptime.load1, m.uptime.uptime_s / 3600.0,
                     outlet->outlet, outlet->watts, outlet->on ? "ON" : "OFF");
        } else {
            snprintf(buf, sizeof(buf), "load %.2f  up %.0fh",
                     m.uptime.load1, m.uptime.uptime_s / 3600.0);
        }
    } else {
        snprintf(buf, sizeof(buf), "offline");
    }
    lv_label_set_text(l_bottom, buf);
    lv_obj_set_style_opa(l_bottom, dim, 0);
}

void HostCardWide::update_or_create(lv_obj_t* parent, const std::string& host,
                                     const HostEntry& entry,
                                     std::optional<protocol::OutletState> outlet,
                                     bool pdu_enabled,
                                     TapCb on_tap) {
    lv_obj_t* card = find_card(parent, host);
    if (!card) card = create_card(parent, host, std::move(on_tap));
    populate(card, entry, outlet, pdu_enabled);
}
