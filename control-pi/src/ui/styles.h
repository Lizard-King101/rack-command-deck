#pragma once
#include <lvgl.h>

namespace styles {
    void init();
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
    static constexpr lv_color_t BG         = LV_COLOR_MAKE(0x08, 0x08, 0x0E);
    static constexpr lv_color_t BG_CARD    = LV_COLOR_MAKE(0x14, 0x10, 0x20);
    static constexpr lv_color_t BG_RAISED  = LV_COLOR_MAKE(0x1A, 0x15, 0x2A);
    static constexpr lv_color_t BG_HEADER  = LV_COLOR_MAKE(0x0D, 0x0A, 0x1A);
    static constexpr lv_color_t ACCENT     = LV_COLOR_MAKE(0x88, 0x44, 0xFF);
    static constexpr lv_color_t ACCENT2    = LV_COLOR_MAKE(0x00, 0xE5, 0xFF);
    static constexpr lv_color_t ACCENT_DIM = LV_COLOR_MAKE(0x44, 0x22, 0x88);
    static constexpr lv_color_t OK         = LV_COLOR_MAKE(0x00, 0xFF, 0x88);
    static constexpr lv_color_t WARN       = LV_COLOR_MAKE(0xFF, 0xAA, 0x00);
    static constexpr lv_color_t DANGER     = LV_COLOR_MAKE(0xFF, 0x22, 0x44);
    static constexpr lv_color_t TEXT       = LV_COLOR_MAKE(0xE8, 0xE0, 0xFF);
    static constexpr lv_color_t TEXT_DIM   = LV_COLOR_MAKE(0x60, 0x55, 0x80);
    static constexpr lv_color_t BORDER     = LV_COLOR_MAKE(0x2A, 0x20, 0x44);
    static constexpr lv_color_t BAR_TRACK  = LV_COLOR_MAKE(0x22, 0x18, 0x38);
} // namespace styles
