#include "styles.h"

lv_color_t styles::BG;
lv_color_t styles::BG_CARD;
lv_color_t styles::BG_RAISED;
lv_color_t styles::BG_HEADER;
lv_color_t styles::ACCENT;
lv_color_t styles::ACCENT2;
lv_color_t styles::ACCENT_DIM;
lv_color_t styles::OK;
lv_color_t styles::WARN;
lv_color_t styles::DANGER;
lv_color_t styles::TEXT;
lv_color_t styles::TEXT_DIM;
lv_color_t styles::BORDER;
lv_color_t styles::BAR_TRACK;

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
static ThemeProfile s_theme;

static lv_color_t color(uint32_t rgb) {
    return lv_color_make((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

const ThemeProfile& styles::theme() { return s_theme; }

static void load_palette(const ThemeProfile& p) {
    styles::BG = color(p.bg); styles::BG_CARD = color(p.bg_card);
    styles::BG_RAISED = color(p.bg_raised); styles::BG_HEADER = color(p.bg_header);
    styles::ACCENT = color(p.accent); styles::ACCENT2 = color(p.accent2);
    styles::ACCENT_DIM = color(p.accent_dim); styles::OK = color(p.ok);
    styles::WARN = color(p.warn); styles::DANGER = color(p.danger);
    styles::TEXT = color(p.text); styles::TEXT_DIM = color(p.text_dim);
    styles::BORDER = color(p.border); styles::BAR_TRACK = color(p.bar_track);
}

static void reset_styles() {
    lv_style_t* all[] = {
        &styles::header_bar, &styles::tab_bar, &styles::tab_btn_active,
        &styles::tab_btn_inactive, &styles::card_wide, &styles::card_wide_pressed,
        &styles::panel, &styles::metric_tile, &styles::status_strip_ok,
        &styles::status_strip_warn, &styles::status_strip_danger,
        &styles::status_strip_offline, &styles::btn_action, &styles::btn_danger,
        &styles::btn_action_pressed, &styles::section_label, &styles::bar_track,
        &styles::bar_cpu_ind, &styles::bar_ram_ind, &styles::bar_temp_ind,
        &styles::chart_bg, &styles::label_title, &styles::label_value,
        &styles::label_secondary, &styles::label_warn, &styles::label_danger,
        &styles::label_ok, &styles::label_badge
    };
    for (auto* style : all) lv_style_reset(style);
}

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

void styles::apply(const ThemeProfile& profile) {
    if (s_initted) {
        reset_styles();
        s_initted = false;
    }
    init(profile);
    lv_obj_report_style_change(nullptr);
}

void styles::init(const ThemeProfile& profile) {
    if (s_initted) return;
    s_initted = true;
    s_theme = profile;
    load_palette(profile);
    const int density_pad = profile.density * 2;

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
    lv_style_set_bg_color(&tab_btn_active, BG_RAISED);
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
    lv_style_set_radius(&card_wide, profile.radius);
    lv_style_set_border_width(&card_wide, 1);
    lv_style_set_border_color(&card_wide, BORDER);
    lv_style_set_pad_all(&card_wide, 0);

    lv_style_init(&card_wide_pressed);
    lv_style_set_bg_color(&card_wide_pressed, ACCENT_DIM);

    lv_style_init(&panel);
    lv_style_set_bg_color(&panel, BG_CARD);
    lv_style_set_bg_opa(&panel, LV_OPA_COVER);
    lv_style_set_radius(&panel, profile.radius + 2);
    lv_style_set_border_width(&panel, 1);
    lv_style_set_border_color(&panel, BORDER);
    lv_style_set_pad_all(&panel, 10 + density_pad);
    lv_style_set_shadow_width(&panel, profile.shadow);
    lv_style_set_shadow_color(&panel, BG);
    lv_style_set_shadow_opa(&panel, LV_OPA_40);

    lv_style_init(&metric_tile);
    lv_style_set_bg_color(&metric_tile, BG_RAISED);
    lv_style_set_bg_opa(&metric_tile, LV_OPA_COVER);
    lv_style_set_radius(&metric_tile, profile.radius);
    lv_style_set_border_width(&metric_tile, 1);
    lv_style_set_border_color(&metric_tile, BORDER);
    lv_style_set_pad_all(&metric_tile, 8 + density_pad);

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
    lv_style_set_radius(&btn_action, profile.radius);
    lv_style_set_border_width(&btn_action, 1);
    lv_style_set_border_color(&btn_action, ACCENT);
    lv_style_set_pad_hor(&btn_action, 12);
    lv_style_set_pad_ver(&btn_action, 6);

    lv_style_init(&btn_action_pressed);
    lv_style_set_bg_color(&btn_action_pressed, ACCENT);
    lv_style_set_text_color(&btn_action_pressed, BG);

    lv_style_init(&btn_danger);
    lv_style_set_bg_color(&btn_danger, BG_RAISED);
    lv_style_set_bg_opa(&btn_danger, LV_OPA_COVER);
    lv_style_set_text_color(&btn_danger, DANGER);
    lv_style_set_text_font(&btn_danger, &lv_font_montserrat_14);
    lv_style_set_radius(&btn_danger, profile.radius);
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
    lv_style_set_radius(&chart_bg, profile.radius);
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
