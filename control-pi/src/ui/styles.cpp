#include "styles.h"

lv_style_t styles::header_bar;
lv_style_t styles::tab_bar;
lv_style_t styles::tab_btn_active;
lv_style_t styles::tab_btn_inactive;
lv_style_t styles::card_wide;
lv_style_t styles::card_wide_pressed;
lv_style_t styles::panel;
lv_style_t styles::metric_tile;
lv_style_t styles::status_strip_ok;
lv_style_t styles::status_strip_warn;
lv_style_t styles::status_strip_danger;
lv_style_t styles::status_strip_offline;
lv_style_t styles::btn_action;
lv_style_t styles::btn_danger;
lv_style_t styles::btn_action_pressed;
lv_style_t styles::section_label;
lv_style_t styles::bar_track;
lv_style_t styles::bar_cpu_ind;
lv_style_t styles::bar_ram_ind;
lv_style_t styles::bar_temp_ind;
lv_style_t styles::chart_bg;
lv_style_t styles::label_title;
lv_style_t styles::label_value;
lv_style_t styles::label_secondary;
lv_style_t styles::label_warn;
lv_style_t styles::label_danger;
lv_style_t styles::label_ok;
lv_style_t styles::label_badge;

static bool s_initted = false;

void styles::make_static(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_scroll_dir(obj, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void styles::make_scroll_passthrough(lv_obj_t* obj) {
    make_static(obj);
    uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i)
        make_scroll_passthrough(lv_obj_get_child(obj, i));
}

void styles::make_vertical_scroll(lv_obj_t* obj) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scroll_dir(obj, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
}

void styles::init() {
    if (s_initted) return;
    s_initted = true;

    // Header bar
    lv_style_init(&header_bar);
    lv_style_set_bg_color(&header_bar, BG_HEADER);
    lv_style_set_bg_opa(&header_bar, LV_OPA_COVER);
    lv_style_set_border_width(&header_bar, 0);
    lv_style_set_border_side(&header_bar, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&header_bar, BORDER);
    lv_style_set_border_width(&header_bar, 1);
    lv_style_set_radius(&header_bar, 0);
    lv_style_set_pad_hor(&header_bar, 12);
    lv_style_set_pad_ver(&header_bar, 0);

    // Tab bar
    lv_style_init(&tab_bar);
    lv_style_set_bg_color(&tab_bar, BG_HEADER);
    lv_style_set_bg_opa(&tab_bar, LV_OPA_COVER);
    lv_style_set_border_width(&tab_bar, 1);
    lv_style_set_border_side(&tab_bar, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&tab_bar, BORDER);
    lv_style_set_radius(&tab_bar, 0);
    lv_style_set_pad_all(&tab_bar, 0);

    // Tab buttons
    lv_style_init(&tab_btn_active);
    lv_style_set_text_color(&tab_btn_active, ACCENT);
    lv_style_set_text_font(&tab_btn_active, &lv_font_montserrat_14);
    lv_style_set_border_width(&tab_btn_active, 2);
    lv_style_set_border_side(&tab_btn_active, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&tab_btn_active, ACCENT);
    lv_style_set_bg_color(&tab_btn_active, LV_COLOR_MAKE(0x18, 0x10, 0x30));
    lv_style_set_bg_opa(&tab_btn_active, LV_OPA_COVER);
    lv_style_set_radius(&tab_btn_active, 0);

    lv_style_init(&tab_btn_inactive);
    lv_style_set_text_color(&tab_btn_inactive, TEXT_DIM);
    lv_style_set_text_font(&tab_btn_inactive, &lv_font_montserrat_14);
    lv_style_set_border_width(&tab_btn_inactive, 0);
    lv_style_set_bg_color(&tab_btn_inactive, BG_HEADER);
    lv_style_set_bg_opa(&tab_btn_inactive, LV_OPA_COVER);
    lv_style_set_radius(&tab_btn_inactive, 0);

    // Wide card
    lv_style_init(&card_wide);
    lv_style_set_bg_color(&card_wide, BG_CARD);
    lv_style_set_bg_opa(&card_wide, LV_OPA_COVER);
    lv_style_set_radius(&card_wide, 6);
    lv_style_set_border_width(&card_wide, 1);
    lv_style_set_border_color(&card_wide, BORDER);
    lv_style_set_pad_all(&card_wide, 0);

    lv_style_init(&card_wide_pressed);
    lv_style_set_bg_color(&card_wide_pressed, LV_COLOR_MAKE(0x20, 0x18, 0x38));

    lv_style_init(&panel);
    lv_style_set_bg_color(&panel, BG_CARD);
    lv_style_set_bg_opa(&panel, LV_OPA_COVER);
    lv_style_set_radius(&panel, 8);
    lv_style_set_border_width(&panel, 1);
    lv_style_set_border_color(&panel, BORDER);
    lv_style_set_pad_all(&panel, 10);
    lv_style_set_shadow_width(&panel, 8);
    lv_style_set_shadow_color(&panel, BG);
    lv_style_set_shadow_opa(&panel, LV_OPA_40);

    lv_style_init(&metric_tile);
    lv_style_set_bg_color(&metric_tile, BG_RAISED);
    lv_style_set_bg_opa(&metric_tile, LV_OPA_COVER);
    lv_style_set_radius(&metric_tile, 6);
    lv_style_set_border_width(&metric_tile, 1);
    lv_style_set_border_color(&metric_tile, BORDER);
    lv_style_set_pad_all(&metric_tile, 8);

    // Status strips (4px left border of cards)
    lv_style_init(&status_strip_ok);
    lv_style_set_bg_color(&status_strip_ok, OK);
    lv_style_set_bg_opa(&status_strip_ok, LV_OPA_COVER);
    lv_style_set_border_width(&status_strip_ok, 0);
    lv_style_set_radius(&status_strip_ok, 0);

    lv_style_init(&status_strip_warn);
    lv_style_set_bg_color(&status_strip_warn, WARN);
    lv_style_set_bg_opa(&status_strip_warn, LV_OPA_COVER);
    lv_style_set_border_width(&status_strip_warn, 0);
    lv_style_set_radius(&status_strip_warn, 0);

    lv_style_init(&status_strip_danger);
    lv_style_set_bg_color(&status_strip_danger, DANGER);
    lv_style_set_bg_opa(&status_strip_danger, LV_OPA_COVER);
    lv_style_set_border_width(&status_strip_danger, 0);
    lv_style_set_radius(&status_strip_danger, 0);

    lv_style_init(&status_strip_offline);
    lv_style_set_bg_color(&status_strip_offline, TEXT_DIM);
    lv_style_set_bg_opa(&status_strip_offline, LV_OPA_COVER);
    lv_style_set_border_width(&status_strip_offline, 0);
    lv_style_set_radius(&status_strip_offline, 0);

    // Action buttons
    lv_style_init(&btn_action);
    lv_style_set_bg_color(&btn_action, ACCENT_DIM);
    lv_style_set_bg_opa(&btn_action, LV_OPA_COVER);
    lv_style_set_text_color(&btn_action, TEXT);
    lv_style_set_text_font(&btn_action, &lv_font_montserrat_14);
    lv_style_set_radius(&btn_action, 6);
    lv_style_set_border_width(&btn_action, 1);
    lv_style_set_border_color(&btn_action, ACCENT);
    lv_style_set_pad_hor(&btn_action, 12);
    lv_style_set_pad_ver(&btn_action, 6);

    lv_style_init(&btn_action_pressed);
    lv_style_set_bg_color(&btn_action_pressed, ACCENT);
    lv_style_set_text_color(&btn_action_pressed, BG);

    lv_style_init(&btn_danger);
    lv_style_set_bg_color(&btn_danger, LV_COLOR_MAKE(0x44, 0x08, 0x14));
    lv_style_set_bg_opa(&btn_danger, LV_OPA_COVER);
    lv_style_set_text_color(&btn_danger, DANGER);
    lv_style_set_text_font(&btn_danger, &lv_font_montserrat_14);
    lv_style_set_radius(&btn_danger, 6);
    lv_style_set_border_width(&btn_danger, 1);
    lv_style_set_border_color(&btn_danger, DANGER);
    lv_style_set_pad_hor(&btn_danger, 12);
    lv_style_set_pad_ver(&btn_danger, 6);

    // Section label (dim separator line label)
    lv_style_init(&section_label);
    lv_style_set_text_color(&section_label, TEXT_DIM);
    lv_style_set_text_font(&section_label, &lv_font_montserrat_12);

    // Progress bar track (bg)
    lv_style_init(&bar_track);
    lv_style_set_bg_color(&bar_track, BAR_TRACK);
    lv_style_set_bg_opa(&bar_track, LV_OPA_COVER);
    lv_style_set_radius(&bar_track, 3);
    lv_style_set_border_width(&bar_track, 0);

    // Progress bar indicators
    lv_style_init(&bar_cpu_ind);
    lv_style_set_bg_color(&bar_cpu_ind, ACCENT2);
    lv_style_set_bg_opa(&bar_cpu_ind, LV_OPA_COVER);
    lv_style_set_radius(&bar_cpu_ind, 3);

    lv_style_init(&bar_ram_ind);
    lv_style_set_bg_color(&bar_ram_ind, ACCENT);
    lv_style_set_bg_opa(&bar_ram_ind, LV_OPA_COVER);
    lv_style_set_radius(&bar_ram_ind, 3);

    lv_style_init(&bar_temp_ind);
    lv_style_set_bg_color(&bar_temp_ind, WARN);
    lv_style_set_bg_opa(&bar_temp_ind, LV_OPA_COVER);
    lv_style_set_radius(&bar_temp_ind, 3);

    // Chart background
    lv_style_init(&chart_bg);
    lv_style_set_bg_color(&chart_bg, BG_CARD);
    lv_style_set_bg_opa(&chart_bg, LV_OPA_COVER);
    lv_style_set_border_width(&chart_bg, 1);
    lv_style_set_border_color(&chart_bg, BORDER);
    lv_style_set_radius(&chart_bg, 4);
    lv_style_set_pad_all(&chart_bg, 4);
    lv_style_set_line_color(&chart_bg, BORDER);  // grid lines

    // Labels
    lv_style_init(&label_title);
    lv_style_set_text_color(&label_title, TEXT);
    lv_style_set_text_font(&label_title, &lv_font_montserrat_20);

    lv_style_init(&label_value);
    lv_style_set_text_color(&label_value, ACCENT2);
    lv_style_set_text_font(&label_value, &lv_font_montserrat_14);

    lv_style_init(&label_secondary);
    lv_style_set_text_color(&label_secondary, TEXT_DIM);
    lv_style_set_text_font(&label_secondary, &lv_font_montserrat_12);

    lv_style_init(&label_warn);
    lv_style_set_text_color(&label_warn, WARN);
    lv_style_set_text_font(&label_warn, &lv_font_montserrat_12);

    lv_style_init(&label_danger);
    lv_style_set_text_color(&label_danger, DANGER);
    lv_style_set_text_font(&label_danger, &lv_font_montserrat_14);

    lv_style_init(&label_ok);
    lv_style_set_text_color(&label_ok, OK);
    lv_style_set_text_font(&label_ok, &lv_font_montserrat_12);

    lv_style_init(&label_badge);
    lv_style_set_text_color(&label_badge, TEXT);
    lv_style_set_text_font(&label_badge, &lv_font_montserrat_12);
    lv_style_set_bg_color(&label_badge, ACCENT_DIM);
    lv_style_set_bg_opa(&label_badge, LV_OPA_COVER);
    lv_style_set_radius(&label_badge, 4);
    lv_style_set_pad_hor(&label_badge, 4);
    lv_style_set_pad_ver(&label_badge, 2);
}
