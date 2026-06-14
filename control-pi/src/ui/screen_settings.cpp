#include "screen_settings.h"
#include "styles.h"
#include <algorithm>
#include <cstdio>

static lv_obj_t* make_section(lv_obj_t* parent, const char* title) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_add_style(lbl, &styles::label_value, 0);
    lv_obj_set_style_text_color(lbl, styles::TEXT, 0);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_label_set_text(lbl, title);
    return lbl;
}

static void add_kv(lv_obj_t* parent, const char* key, const char* val) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    styles::make_static(row);

    lv_obj_t* lk = lv_label_create(row);
    lv_obj_add_style(lk, &styles::label_secondary, 0);
    lv_obj_set_pos(lk, 8, 0);
    lv_obj_set_width(lk, 200);
    lv_label_set_text(lk, key);

    lv_obj_t* lv = lv_label_create(row);
    lv_obj_add_style(lv, &styles::label_secondary, 0);
    lv_obj_set_style_text_color(lv, styles::TEXT, 0);
    lv_obj_set_pos(lv, 216, 0);
    lv_obj_set_width(lv, 400);
    lv_label_set_long_mode(lv, LV_LABEL_LONG_DOT);
    lv_label_set_text(lv, val);
}

static lv_obj_t* studio_button(lv_obj_t* parent, const char* text, int width,
                               std::function<void()> cb);

ScreenSettings::ScreenSettings(lv_obj_t* parent, const Config& cfg, ActivityStore& activity,
                               UpdateManager& updater, ThemeStore& themes,
                               std::function<void(const ThemeProfile&)> on_apply)
    : cfg_(cfg), activity_(activity), updater_(updater), themes_(themes),
      on_apply_(std::move(on_apply)), profiles_(themes.profiles()), working_(themes.active()) {
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, 800, 390);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, styles::BG, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    styles::make_static(container_);

    main_page_ = lv_obj_create(container_);
    lv_obj_set_size(main_page_, 800, 390);
    lv_obj_set_pos(main_page_, 0, 0);
    lv_obj_set_style_bg_opa(main_page_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_page_, 0, 0);
    lv_obj_set_style_pad_all(main_page_, 0, 0);
    styles::make_static(main_page_);

    lv_obj_t* scroll = lv_obj_create(main_page_);
    lv_obj_set_size(scroll, 800, 390);
    lv_obj_set_pos(scroll, 0, 0);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 12, 0);
    lv_obj_set_style_pad_row(scroll, 6, 0);
    styles::make_vertical_scroll(scroll);

    auto make_panel = [&]() {
        lv_obj_t* panel = lv_obj_create(scroll);
        lv_obj_set_width(panel, LV_PCT(100));
        lv_obj_set_height(panel, LV_SIZE_CONTENT);
        lv_obj_add_style(panel, &styles::panel, 0);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(panel, 5, 0);
        styles::make_static(panel);
        return panel;
    };

    lv_obj_t* appearance = make_panel();
    make_section(appearance, "APPEARANCE & THEME STUDIO");
    auto* appearance_hint = lv_label_create(appearance);
    lv_obj_add_style(appearance_hint, &styles::label_secondary, 0);
    lv_obj_set_width(appearance_hint, LV_PCT(100));
    lv_label_set_text(appearance_hint,
        "Customize branding, colors, surfaces, motion, density, and clock format.");
    lbl_active_theme_ = lv_label_create(appearance);
    lv_obj_add_style(lbl_active_theme_, &styles::label_value, 0);
    lv_label_set_text(lbl_active_theme_, working_.name.c_str());
    studio_button(appearance, "OPEN THEME STUDIO  >", 230, [this] { show_theme_studio(); });

    // NETWORK section
    lv_obj_t* network = make_panel();
    make_section(network, "CONTROL SERVER");
    char buf[128];
    snprintf(buf, sizeof(buf), "%d", cfg_.server.port);
    add_kv(network, "WebSocket Port", buf);
    add_kv(network, "Bind Address", cfg_.server.bind.c_str());
    add_kv(network, "Host Cache", cfg_.server.cache_file.c_str());
    lbl_agents_ = lv_label_create(network);
    lv_obj_add_style(lbl_agents_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_agents_, LV_PCT(100));
    lv_label_set_text(lbl_agents_, "Agents: 0 online / 0 known");

    // DISPLAY section
    lv_obj_t* display = make_panel();
    make_section(display, "DISPLAY");
    snprintf(buf, sizeof(buf), "%dx%d", cfg_.display.width, cfg_.display.height);
    add_kv(display, "Resolution", buf);
    snprintf(buf, sizeof(buf), "%d", cfg_.display.brightness);
    add_kv(display, "Brightness", buf);
    add_kv(display, "Framebuffer", cfg_.display.fb_device.c_str());
    const int active_saver_s = working_.screensaver_s < 0
        ? cfg_.display.screensaver_s : working_.screensaver_s;
    snprintf(buf, sizeof(buf), active_saver_s > 0 ? "%d seconds" : "disabled", active_saver_s);
    add_kv(display, "Rack Status Screensaver", buf);

    // PDU section
    lv_obj_t* power = make_panel();
    make_section(power, "POWER CONTROLLER");
    add_kv(power, "Enabled", cfg_.pdu.enabled ? "yes" : "no");
    if (cfg_.pdu.enabled) {
        add_kv(power, "Host", cfg_.pdu.host.c_str());
        snprintf(buf, sizeof(buf), "%d", cfg_.pdu.port);
        add_kv(power, "Port", buf);
    }
    add_kv(power, "History DB", cfg_.power.database_path.c_str());
    snprintf(buf, sizeof(buf), "%.0f W / %.0f W", cfg_.power.warning_watts,
             cfg_.power.critical_watts);
    add_kv(power, "Warning / Critical", buf);
    snprintf(buf, sizeof(buf), "%s %.4f per kWh", cfg_.power.currency.c_str(),
             cfg_.power.cost_per_kwh);
    add_kv(power, "Energy Cost", buf);
    add_kv(power, "Auto Shedding", cfg_.power.load_shedding_enabled ? "enabled" : "disabled");
    snprintf(buf, sizeof(buf), "%zu", cfg_.power_groups.size());
    add_kv(power, "Power Groups", buf);

    lv_obj_t* update_panel = make_panel();
    make_section(update_panel, "DASHBOARD UPDATE");
    add_kv(update_panel, "Release", cfg_.update.release_url.empty() ? "not configured" :
           cfg_.update.release_url.c_str());

    btn_update_ = lv_button_create(update_panel);
    lv_obj_add_style(btn_update_, &styles::btn_action, 0);
    lv_obj_add_style(btn_update_, &styles::btn_action_pressed, LV_STATE_PRESSED);
    styles::make_static(btn_update_);
    lv_obj_t* update_btn_label = lv_label_create(btn_update_);
    lv_label_set_text(update_btn_label, "DOWNLOAD, INSTALL, AND RESTART");
    lv_obj_center(update_btn_label);
    lv_obj_add_event_cb(btn_update_, +[](lv_event_t* event) {
        static_cast<ScreenSettings*>(lv_event_get_user_data(event))->updater_.start();
    }, LV_EVENT_CLICKED, this);
    if (!updater_.available()) lv_obj_add_state(btn_update_, LV_STATE_DISABLED);

    bar_update_ = lv_bar_create(update_panel);
    lv_obj_set_size(bar_update_, LV_PCT(100), 8);
    lv_bar_set_range(bar_update_, 0, 100);
    lv_obj_add_style(bar_update_, &styles::bar_track, LV_PART_MAIN);
    lv_obj_add_style(bar_update_, &styles::bar_cpu_ind, LV_PART_INDICATOR);
    lv_bar_set_value(bar_update_, 0, LV_ANIM_OFF);

    lbl_update_ = lv_label_create(update_panel);
    lv_obj_add_style(lbl_update_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_update_, LV_PCT(100));
    lv_label_set_text(lbl_update_, updater_.available() ? "Ready to update" :
                      "Updater is disabled or not configured");

    lv_obj_t* automation = make_panel();
    make_section(automation, "AUTOMATION");
    lv_obj_t* hint = lv_label_create(automation);
    lv_obj_add_style(hint, &styles::label_secondary, 0);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_label_set_text(hint, "Scripts and services are defined per-machine in agent.toml");

    lv_obj_t* activity_panel = make_panel();
    make_section(activity_panel, "RECENT COMMANDS");
    lbl_activity_ = lv_label_create(activity_panel);
    lv_obj_add_style(lbl_activity_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_activity_, LV_PCT(100));
    lv_label_set_text(lbl_activity_, "No commands have been sent.");

    uint32_t count = lv_obj_get_child_count(scroll);
    for (uint32_t i = 0; i < count; ++i)
        styles::make_scroll_passthrough(lv_obj_get_child(scroll, i));

    theme_page_ = lv_obj_create(container_);
    lv_obj_set_size(theme_page_, 800, 390);
    lv_obj_set_pos(theme_page_, 0, 0);
    lv_obj_set_style_bg_color(theme_page_, styles::BG, 0);
    lv_obj_set_style_bg_opa(theme_page_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(theme_page_, 0, 0);
    lv_obj_set_style_pad_all(theme_page_, 0, 0);
    styles::make_static(theme_page_);
    lv_obj_add_flag(theme_page_, LV_OBJ_FLAG_HIDDEN);
    build_theme_studio(theme_page_);
}

static lv_obj_t* studio_button(lv_obj_t* parent, const char* text, int width,
                               std::function<void()> cb) {
    auto* btn = lv_button_create(parent);
    lv_obj_set_size(btn, width, 32);
    lv_obj_add_style(btn, &styles::btn_action, 0);
    lv_obj_add_style(btn, &styles::btn_action_pressed, LV_STATE_PRESSED);
    styles::make_static(btn);
    auto* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    auto* fn = new std::function<void()>(std::move(cb));
    lv_obj_add_event_cb(btn, +[](lv_event_t* event) {
        (*static_cast<std::function<void()>*>(lv_event_get_user_data(event)))();
    }, LV_EVENT_CLICKED, fn);
    return btn;
}

void ScreenSettings::build_theme_studio(lv_obj_t* parent) {
    auto* header = lv_obj_create(parent);
    lv_obj_set_size(header, 800, 42);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, styles::BG_HEADER, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, styles::BORDER, 0);
    lv_obj_set_style_radius(header, 0, 0);
    styles::make_static(header);
    auto* back = studio_button(header, "< SYSTEM", 110, [this] { hide_theme_studio(); });
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
    auto* page_title = lv_label_create(header);
    lv_obj_add_style(page_title, &styles::label_value, 0);
    lv_obj_set_style_text_color(page_title, styles::TEXT, 0);
    lv_label_set_text(page_title, "THEME STUDIO");
    lv_obj_align(page_title, LV_ALIGN_CENTER, 0, 0);

    auto* scroll = lv_obj_create(parent);
    lv_obj_set_size(scroll, 800, 348);
    lv_obj_set_pos(scroll, 0, 42);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 10, 0);
    lv_obj_set_style_pad_row(scroll, 8, 0);
    styles::make_vertical_scroll(scroll);

    auto* panel = lv_obj_create(scroll);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_add_style(panel, &styles::panel, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 7, 0);
    styles::make_static(panel);
    make_section(panel, "PROFILE & LIVE PREVIEW");

    profile_dropdown_ = lv_dropdown_create(panel);
    lv_obj_set_width(profile_dropdown_, LV_PCT(100));
    lv_obj_set_style_bg_color(profile_dropdown_, styles::BG_RAISED, 0);
    lv_obj_set_style_text_color(profile_dropdown_, styles::TEXT, 0);
    lv_obj_set_style_border_color(profile_dropdown_, styles::BORDER, 0);
    lv_obj_set_style_border_width(profile_dropdown_, 1, 0);
    lv_obj_set_style_radius(profile_dropdown_, styles::theme().radius, 0);
    rebuild_profile_options();
    lv_obj_add_event_cb(profile_dropdown_, +[](lv_event_t* event) {
        auto* self = static_cast<ScreenSettings*>(lv_event_get_user_data(event));
        self->select_profile(lv_dropdown_get_selected(self->profile_dropdown_));
    }, LV_EVENT_VALUE_CHANGED, this);

    preview_ = lv_obj_create(panel);
    lv_obj_set_size(preview_, LV_PCT(100), 78);
    lv_obj_set_style_pad_all(preview_, 8, 0);
    styles::make_static(preview_);
    preview_mark_ = lv_label_create(preview_);
    lv_obj_set_pos(preview_mark_, 0, 0);
    preview_title_ = lv_label_create(preview_);
    lv_obj_set_pos(preview_title_, 50, 0);
    preview_meta_ = lv_label_create(preview_);
    lv_obj_set_pos(preview_meta_, 50, 24);
    auto swatch = [&](int x) {
        auto* obj = lv_obj_create(preview_);
        lv_obj_set_size(obj, 44, 12);
        lv_obj_set_pos(obj, x, 48);
        lv_obj_set_style_border_width(obj, 0, 0);
        styles::make_static(obj);
        return obj;
    };
    preview_accent_ = swatch(50); preview_ok_ = swatch(102);
    preview_warn_ = swatch(154); preview_danger_ = swatch(206);

    auto* branding = lv_obj_create(panel);
    lv_obj_set_width(branding, LV_PCT(100));
    lv_obj_set_height(branding, 42);
    lv_obj_set_flex_flow(branding, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(branding, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(branding, 0, 0);
    lv_obj_set_style_pad_all(branding, 0, 0);
    lv_obj_set_style_pad_gap(branding, 6, 0);
    styles::make_static(branding);
    auto make_brand_field = [&](const char* placeholder, const std::string& value, lv_obj_t** field) {
        *field = lv_textarea_create(branding);
        lv_obj_set_size(*field, 360, 38);
        lv_textarea_set_one_line(*field, true);
        lv_textarea_set_placeholder_text(*field, placeholder);
        lv_textarea_set_text(*field, value.c_str());
        lv_obj_set_style_bg_color(*field, styles::BG_RAISED, 0);
        lv_obj_set_style_text_color(*field, styles::TEXT, 0);
        lv_obj_set_style_border_color(*field, styles::BORDER, 0);
        lv_obj_set_style_border_width(*field, 1, 0);
        lv_obj_set_style_radius(*field, styles::theme().radius, 0);
        lv_obj_add_event_cb(*field, +[](lv_event_t* event) {
            auto* self = static_cast<ScreenSettings*>(lv_event_get_user_data(event));
            self->working_.title = lv_textarea_get_text(self->brand_title_);
            self->working_.subtitle = lv_textarea_get_text(self->brand_subtitle_);
            self->refresh_theme_preview();
        }, LV_EVENT_VALUE_CHANGED, this);
        lv_obj_add_event_cb(*field, +[](lv_event_t* event) {
            auto* self = static_cast<ScreenSettings*>(lv_event_get_user_data(event));
            lv_keyboard_set_textarea(self->theme_keyboard_, lv_event_get_target_obj(event));
            lv_obj_clear_flag(self->theme_keyboard_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(self->theme_keyboard_);
        }, LV_EVENT_FOCUSED, this);
    };
    make_brand_field("Deck title", working_.title, &brand_title_);
    make_brand_field("Subtitle", working_.subtitle, &brand_subtitle_);

    auto* controls_panel = lv_obj_create(scroll);
    lv_obj_set_width(controls_panel, LV_PCT(100));
    lv_obj_set_height(controls_panel, LV_SIZE_CONTENT);
    lv_obj_add_style(controls_panel, &styles::panel, 0);
    lv_obj_set_flex_flow(controls_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(controls_panel, 6, 0);
    styles::make_static(controls_panel);
    make_section(controls_panel, "STYLE CONTROLS");

    auto make_control = [&](const char* title, const char* description, lv_obj_t** value,
                            std::function<void()> cb) {
        auto* row = lv_obj_create(controls_panel);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 54);
        lv_obj_set_style_bg_color(row, styles::BG_RAISED, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, styles::BORDER, 0);
        lv_obj_set_style_radius(row, styles::theme().radius, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        styles::make_static(row);
        auto* title_label = lv_label_create(row);
        lv_obj_add_style(title_label, &styles::label_value, 0);
        lv_obj_set_style_text_color(title_label, styles::TEXT, 0);
        lv_obj_set_pos(title_label, 0, 0);
        lv_label_set_text(title_label, title);
        auto* hint = lv_label_create(row);
        lv_obj_add_style(hint, &styles::label_secondary, 0);
        lv_obj_set_pos(hint, 0, 24);
        lv_label_set_text(hint, description);
        *value = lv_label_create(row);
        lv_obj_add_style(*value, &styles::label_value, 0);
        lv_obj_set_style_text_color(*value, styles::ACCENT2, 0);
        lv_obj_set_width(*value, 150);
        lv_obj_set_style_text_align(*value, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(*value, 450, 10);
        auto* button = studio_button(row, "CHANGE", 105, std::move(cb));
        lv_obj_align(button, LV_ALIGN_RIGHT_MID, 0, 0);
    };
    make_control("Accent Palette", "Primary and secondary interactive colors",
                 &lbl_accent_value_, [this] { cycle_accent(); });
    make_control("Status Palette", "Healthy, warning, and critical colors",
                 &lbl_status_value_, [this] { cycle_status(); });
    make_control("Header Mark", "Symbol shown beside the deck title",
                 &lbl_mark_value_, [this] { cycle_mark(); });
    make_control("Corner Radius", "Shape of cards, panels, and buttons",
                 &lbl_radius_value_, [this] {
                     working_.radius = working_.radius >= 12 ? 0 : working_.radius + 4;
                     refresh_theme_preview();
                 });
    make_control("Panel Shadows", "Depth around raised panels",
                 &lbl_shadow_value_, [this] {
                     working_.shadow = working_.shadow ? 0 : 8; refresh_theme_preview();
                 });
    make_control("Interface Density", "Padding inside reusable UI components",
                 &lbl_density_value_, [this] {
                     working_.density = working_.density >= 1 ? -1 : working_.density + 1;
                     refresh_theme_preview();
                 });
    make_control("Motion", "Blinking cursor and animated progress",
                 &lbl_motion_value_, [this] {
                     working_.motion = !working_.motion; refresh_theme_preview();
                 });
    make_control("Clock Format", "Header and screensaver time display",
                 &lbl_clock_value_, [this] {
                     working_.clock_24h = !working_.clock_24h; refresh_theme_preview();
                 });
    make_control("Rack Status Screensaver", "Show rack health after a period without touch input",
                 &lbl_screensaver_value_, [this] {
                     const int values[] = {-1, 0, 60, 300, 900};
                     size_t current = 0;
                     for (size_t i = 0; i < std::size(values); ++i)
                         if (working_.screensaver_s == values[i]) current = i + 1;
                     working_.screensaver_s = values[current % std::size(values)];
                     refresh_theme_preview();
                 });
    make_control("Animated Background", "Use the GIF path configured in the profile TOML",
                 &lbl_saver_background_value_, [this] {
                     working_.screensaver_background_enabled =
                         !working_.screensaver_background_enabled;
                     refresh_theme_preview();
                 });
    make_control("Background Veil", "Theme-colored overlay that keeps status text readable",
                 &lbl_saver_veil_value_, [this] {
                     const int values[] = {20, 40, 55, 70, 85};
                     size_t current = 0;
                     for (size_t i = 0; i < std::size(values); ++i)
                         if (working_.screensaver_veil == values[i]) current = i + 1;
                     working_.screensaver_veil = values[current % std::size(values)];
                     refresh_theme_preview();
                 });
    make_control("Status Card Opacity", "Lower values reveal more of the animated background",
                 &lbl_saver_card_opacity_value_, [this] {
                     const int values[] = {35, 50, 65, 80, 90, 100};
                     size_t current = 0;
                     for (size_t i = 0; i < std::size(values); ++i)
                         if (working_.screensaver_card_opacity == values[i]) current = i + 1;
                     working_.screensaver_card_opacity = values[current % std::size(values)];
                     refresh_theme_preview();
                 });

    auto* actions_panel = lv_obj_create(scroll);
    lv_obj_set_width(actions_panel, LV_PCT(100));
    lv_obj_set_height(actions_panel, LV_SIZE_CONTENT);
    lv_obj_add_style(actions_panel, &styles::panel, 0);
    lv_obj_set_flex_flow(actions_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(actions_panel, 6, 0);
    styles::make_static(actions_panel);
    make_section(actions_panel, "PROFILE ACTIONS");
    auto* controls = lv_obj_create(actions_panel);
    lv_obj_set_width(controls, LV_PCT(100));
    lv_obj_set_height(controls, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_style_pad_gap(controls, 5, 0);
    styles::make_static(controls);
    studio_button(controls, "APPLY PROFILE", 145, [this] { apply_profile(); });
    studio_button(controls, "DUPLICATE", 125, [this] { duplicate_profile(); });
    studio_button(controls, "EXPORT TOML", 125, [this] { export_profile(); });
    studio_button(controls, "RELOAD FILES", 125, [this] {
        profiles_ = themes_.profiles(); rebuild_profile_options(); refresh_theme_preview();
    });
    studio_button(controls, "DELETE COPY", 125, [this] { delete_profile(); });

    lbl_theme_status_ = lv_label_create(actions_panel);
    lv_obj_add_style(lbl_theme_status_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_theme_status_, LV_PCT(100));
    lv_label_set_text(lbl_theme_status_, "Detailed branding and color values are editable in profile TOML.");

    theme_keyboard_ = lv_keyboard_create(theme_page_);
    lv_obj_set_size(theme_keyboard_, 800, 220);
    lv_obj_align(theme_keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(theme_keyboard_, LV_OBJ_FLAG_HIDDEN);
    auto hide_keyboard = +[](lv_event_t* event) {
        auto* self = static_cast<ScreenSettings*>(lv_event_get_user_data(event));
        lv_obj_add_flag(self->theme_keyboard_, LV_OBJ_FLAG_HIDDEN);
    };
    lv_obj_add_event_cb(theme_keyboard_, hide_keyboard, LV_EVENT_READY, this);
    lv_obj_add_event_cb(theme_keyboard_, hide_keyboard, LV_EVENT_CANCEL, this);
    uint32_t count = lv_obj_get_child_count(scroll);
    for (uint32_t i = 0; i < count; ++i)
        styles::make_scroll_passthrough(lv_obj_get_child(scroll, i));
    refresh_theme_preview();
}

void ScreenSettings::show_theme_studio() {
    lv_obj_add_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(theme_page_, LV_OBJ_FLAG_HIDDEN);
}

void ScreenSettings::hide_theme_studio() {
    if (theme_keyboard_) lv_obj_add_flag(theme_keyboard_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(theme_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
}

void ScreenSettings::rebuild_profile_options() {
    if (!profile_dropdown_) return;
    std::string options;
    int selected = 0;
    for (size_t i = 0; i < profiles_.size(); ++i) {
        if (i) options += "\n";
        options += profiles_[i].name;
        if (profiles_[i].id == working_.id) selected = (int)i;
    }
    lv_dropdown_set_options(profile_dropdown_, options.c_str());
    lv_dropdown_set_selected(profile_dropdown_, selected);
}

void ScreenSettings::select_profile(size_t index) {
    if (index >= profiles_.size()) return;
    working_ = profiles_[index];
    if (brand_title_) lv_textarea_set_text(brand_title_, working_.title.c_str());
    if (brand_subtitle_) lv_textarea_set_text(brand_subtitle_, working_.subtitle.c_str());
    refresh_theme_preview();
}

void ScreenSettings::refresh_theme_preview() {
    if (!preview_) return;
    auto color = [](uint32_t rgb) {
        return lv_color_make((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    };
    lv_obj_set_style_bg_color(preview_, color(working_.bg_card), 0);
    lv_obj_set_style_border_color(preview_, color(working_.border), 0);
    lv_obj_set_style_border_width(preview_, 1, 0);
    lv_obj_set_style_radius(preview_, working_.radius, 0);
    lv_obj_set_style_shadow_width(preview_, working_.shadow, 0);
    lv_label_set_text(preview_mark_, theme_mark_text(working_.mark));
    lv_obj_set_style_text_color(preview_mark_, color(working_.accent), 0);
    std::string title = working_.title + (working_.subtitle.empty() ? "" : " // " + working_.subtitle);
    lv_label_set_text(preview_title_, title.c_str());
    lv_obj_set_style_text_color(preview_title_, color(working_.text), 0);
    char meta[160];
    snprintf(meta, sizeof(meta), "%s  |  radius %d  shadow %d  density %d  motion %s  %s",
             working_.name.c_str(), working_.radius, working_.shadow, working_.density,
             working_.motion ? "on" : "reduced", working_.clock_24h ? "24h" : "12h");
    lv_label_set_text(preview_meta_, meta);
    lv_obj_set_style_text_color(preview_meta_, color(working_.text_dim), 0);
    lv_obj_set_style_bg_color(preview_accent_, color(working_.accent), 0);
    lv_obj_set_style_bg_color(preview_ok_, color(working_.ok), 0);
    lv_obj_set_style_bg_color(preview_warn_, color(working_.warn), 0);
    lv_obj_set_style_bg_color(preview_danger_, color(working_.danger), 0);
    if (lbl_accent_value_) {
        const char* value = working_.accent == 0x8844ff ? "Neon" :
                            working_.accent == 0x58a6ff ? "Blue" :
                            working_.accent == 0xff4fa3 ? "Pink / Gold" :
                            working_.accent == 0x00b894 ? "Teal / Blue" : "Custom";
        lv_label_set_text(lbl_accent_value_, value);
    }
    if (lbl_status_value_) {
        const char* value = working_.ok == 0x00ff88 ? "Neon" :
                            working_.ok == 0x3fb950 ? "Natural" :
                            working_.ok == 0x00ffff ? "Contrast" : "Custom";
        lv_label_set_text(lbl_status_value_, value);
    }
    if (lbl_mark_value_) lv_label_set_text(lbl_mark_value_, working_.mark.c_str());
    char value[32];
    if (lbl_radius_value_) {
        snprintf(value, sizeof(value), "%d px", working_.radius);
        lv_label_set_text(lbl_radius_value_, value);
    }
    if (lbl_shadow_value_) lv_label_set_text(lbl_shadow_value_, working_.shadow ? "Enabled" : "Off");
    if (lbl_density_value_) lv_label_set_text(lbl_density_value_,
        working_.density < 0 ? "Compact" : working_.density > 0 ? "Comfortable" : "Standard");
    if (lbl_motion_value_) lv_label_set_text(lbl_motion_value_, working_.motion ? "Standard" : "Reduced");
    if (lbl_clock_value_) lv_label_set_text(lbl_clock_value_, working_.clock_24h ? "24-hour" : "12-hour");
    if (lbl_screensaver_value_) {
        const int seconds = working_.screensaver_s < 0 ? cfg_.display.screensaver_s : working_.screensaver_s;
        const char* value = working_.screensaver_s < 0
                                ? (seconds > 0 ? "System default (on)" : "System default (off)") :
                            seconds == 0 ? "Disabled" :
                            seconds == 60 ? "1 minute" :
                            seconds == 300 ? "5 minutes" :
                            seconds == 900 ? "15 minutes" : "Custom";
        lv_label_set_text(lbl_screensaver_value_, value);
    }
    if (lbl_saver_background_value_) lv_label_set_text(lbl_saver_background_value_,
        !working_.screensaver_background_enabled ? "Disabled" :
        working_.screensaver_background.empty() ? "No GIF configured" : "Enabled");
    if (lbl_saver_veil_value_) {
        char veil[24];
        snprintf(veil, sizeof(veil), "%d%%", working_.screensaver_veil);
        lv_label_set_text(lbl_saver_veil_value_, veil);
    }
    if (lbl_saver_card_opacity_value_) {
        char opacity[24];
        snprintf(opacity, sizeof(opacity), "%d%%", working_.screensaver_card_opacity);
        lv_label_set_text(lbl_saver_card_opacity_value_, opacity);
    }
}

void ScreenSettings::cycle_accent() {
    static const uint32_t pairs[][3] = {
        {0x8844ff, 0x00e5ff, 0x442288}, {0x58a6ff, 0x79c0ff, 0x1f4b7a},
        {0xff4fa3, 0xffd166, 0x6f2348}, {0x00b894, 0x74b9ff, 0x075e54}
    };
    size_t current = 0;
    for (size_t i = 0; i < std::size(pairs); ++i) if (working_.accent == pairs[i][0]) current = i + 1;
    const auto& next = pairs[current % std::size(pairs)];
    working_.accent = next[0]; working_.accent2 = next[1]; working_.accent_dim = next[2];
    refresh_theme_preview();
}

void ScreenSettings::cycle_status() {
    static const uint32_t sets[][3] = {
        {0x00ff88, 0xffaa00, 0xff2244}, {0x3fb950, 0xd29922, 0xf85149},
        {0x00ffff, 0xffff00, 0xff3030}
    };
    size_t current = 0;
    for (size_t i = 0; i < std::size(sets); ++i) if (working_.ok == sets[i][0]) current = i + 1;
    const auto& next = sets[current % std::size(sets)];
    working_.ok = next[0]; working_.warn = next[1]; working_.danger = next[2];
    refresh_theme_preview();
}

void ScreenSettings::cycle_mark() {
    auto marks = theme_mark_ids();
    auto it = std::find(marks.begin(), marks.end(), working_.mark);
    working_.mark = marks[(it == marks.end() ? 0 : (std::distance(marks.begin(), it) + 1) % marks.size())];
    refresh_theme_preview();
}

void ScreenSettings::apply_profile() {
    std::string error;
    if (!themes_.set_active(working_, &error)) {
        lv_label_set_text(lbl_theme_status_, error.c_str());
        return;
    }
    if (on_apply_) on_apply_(working_);
}

void ScreenSettings::duplicate_profile() {
    working_.id = ThemeStore::sanitize_id(working_.id + "-copy");
    working_.name += " Copy";
    std::string error;
    if (themes_.save(working_, &error)) {
        profiles_ = themes_.profiles();
        rebuild_profile_options();
        lv_label_set_text(lbl_theme_status_, "Profile copy saved.");
    } else lv_label_set_text(lbl_theme_status_, error.c_str());
}

void ScreenSettings::delete_profile() {
    std::string error;
    themes_.remove(working_.id, &error);
    profiles_ = themes_.profiles();
    working_ = themes_.active();
    rebuild_profile_options();
    refresh_theme_preview();
    lv_label_set_text(lbl_theme_status_, error.empty() ? "User profile removed; bundled presets remain." : error.c_str());
}

void ScreenSettings::export_profile() {
    std::string path, error;
    if (themes_.export_profile(working_, &path, &error))
        lv_label_set_text(lbl_theme_status_, ("Exported to " + path).c_str());
    else lv_label_set_text(lbl_theme_status_, error.c_str());
}

void ScreenSettings::refresh(const std::map<std::string, HostEntry>& hosts) {
    if (lbl_agents_) {
        int online = 0;
        for (const auto& [name, host] : hosts) {
            (void)name;
            if (host.online) ++online;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "Agents: %d online / %zu known", online, hosts.size());
        lv_label_set_text(lbl_agents_, buf);
    }
    if (bar_update_ && lbl_update_ && btn_update_) {
        auto update = updater_.status();
        lv_bar_set_value(bar_update_, update.progress,
                         styles::theme().motion ? LV_ANIM_ON : LV_ANIM_OFF);
        lv_label_set_text(lbl_update_, update.message.c_str());
        lv_obj_set_style_text_color(lbl_update_,
            update.state == UpdateState::Failed ? styles::DANGER :
            update.state == UpdateState::Succeeded ? styles::OK : styles::TEXT_DIM, 0);
        if (update.state == UpdateState::Running || !updater_.available())
            lv_obj_add_state(btn_update_, LV_STATE_DISABLED);
        else
            lv_obj_remove_state(btn_update_, LV_STATE_DISABLED);
    }
    if (!lbl_activity_) return;
    auto recent = activity_.snapshot(8);
    if (recent.empty()) {
        lv_label_set_text(lbl_activity_, "No commands have been sent.");
        return;
    }
    std::string text;
    for (const auto& a : recent) {
        const char* state = a.state == ActivityState::Pending ? "..." :
                            a.state == ActivityState::Succeeded ? "OK " : "ERR";
        text += state;
        text += "  " + (a.host.empty() ? std::string("deck") : a.host);
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
