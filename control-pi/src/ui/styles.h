#pragma once
#include <lvgl.h>
#include "theme_profile.h"

namespace styles {
    void init(const ThemeProfile& profile);
    void apply(const ThemeProfile& profile);
    const ThemeProfile& theme();
    void make_static(lv_obj_t* obj);
    void make_scroll_passthrough(lv_obj_t* obj);
    void make_vertical_scroll(lv_obj_t* obj);

    // ── Structural styles ───────────────────────────────────────────────────
    extern lv_style_t header_bar;
    extern lv_style_t tab_bar;
    extern lv_style_t tab_btn_active;
    extern lv_style_t tab_btn_inactive;
    extern lv_style_t card_wide;
    extern lv_style_t card_wide_pressed;
    extern lv_style_t panel;
    extern lv_style_t metric_tile;
    extern lv_style_t status_strip_ok;
    extern lv_style_t status_strip_warn;
    extern lv_style_t status_strip_danger;
    extern lv_style_t status_strip_offline;
    extern lv_style_t btn_action;
    extern lv_style_t btn_danger;
    extern lv_style_t btn_action_pressed;
    extern lv_style_t section_label;

    // ── Progress bar styles ─────────────────────────────────────────────────
    extern lv_style_t bar_track;
    extern lv_style_t bar_cpu_ind;
    extern lv_style_t bar_ram_ind;
    extern lv_style_t bar_temp_ind;

    // ── Chart styles ────────────────────────────────────────────────────────
    extern lv_style_t chart_bg;

    // ── Label styles ────────────────────────────────────────────────────────
    extern lv_style_t label_title;     // Montserrat 20, TEXT
    extern lv_style_t label_value;     // Montserrat 14, ACCENT2
    extern lv_style_t label_secondary; // Montserrat 12, TEXT_DIM
    extern lv_style_t label_warn;      // Montserrat 12, WARN
    extern lv_style_t label_danger;    // Montserrat 14, DANGER
    extern lv_style_t label_ok;        // Montserrat 12, OK
    extern lv_style_t label_badge;     // Montserrat 12, TEXT on ACCENT_DIM bg

    // ── Palette ─────────────────────────────────────────────────────────────
    extern lv_color_t BG;
    extern lv_color_t BG_CARD;
    extern lv_color_t BG_RAISED;
    extern lv_color_t BG_HEADER;
    extern lv_color_t ACCENT;
    extern lv_color_t ACCENT2;
    extern lv_color_t ACCENT_DIM;
    extern lv_color_t OK;
    extern lv_color_t WARN;
    extern lv_color_t DANGER;
    extern lv_color_t TEXT;
    extern lv_color_t TEXT_DIM;
    extern lv_color_t BORDER;
    extern lv_color_t BAR_TRACK;
} // namespace styles
