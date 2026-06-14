#include "app_shell.h"
#include "screen_overview.h"
#include "screen_hosts.h"
#include "screen_pdu.h"
#include "screen_settings.h"
#include "screen_host_detail.h"
#include "styles.h"
#include <ctime>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <filesystem>

static AppShell* g_shell = nullptr;

static const char* TAB_LABELS[AppShell::TAB_COUNT] = {
    "OVERVIEW", "HOSTS", "POWER", "SYSTEM"
};

AppShell::AppShell(MetricsStore& store, PduStore& pdu, ActivityStore& activity,
                   CommandRouter& router, PowerHistoryStore& power_history,
                   PowerSequenceEngine& sequences, PowerBudgetController& budgets,
                   UpdateManager& updater, ThemeStore& themes, const Config& cfg)
    : store_(store), pdu_(pdu), activity_(activity), router_(router),
      power_history_(power_history), sequences_(sequences), budgets_(budgets),
      updater_(updater), themes_(themes), cfg_(cfg), theme_(themes.active()) {
    g_shell = this;
}

AppShell::~AppShell() {
    g_shell = nullptr;
}

// ── LVGL timer callbacks ──────────────────────────────────────────────────────

static void shell_refresh_cb (lv_timer_t* t) { static_cast<AppShell*>(lv_timer_get_user_data(t))->refresh(); }
static void shell_clock_cb   (lv_timer_t* t) { static_cast<AppShell*>(lv_timer_get_user_data(t))->on_clock_tick(); }
static void shell_history_cb (lv_timer_t* t) { static_cast<AppShell*>(lv_timer_get_user_data(t))->on_history_tick(); }
static void shell_stale_cb   (lv_timer_t* t) { static_cast<AppShell*>(lv_timer_get_user_data(t))->on_stale_tick(); }
static void shell_cursor_cb  (lv_timer_t* t) { static_cast<AppShell*>(lv_timer_get_user_data(t))->on_cursor_tick(); }
static void shell_power_cb   (lv_timer_t* t) { static_cast<AppShell*>(lv_timer_get_user_data(t))->on_power_tick(); }
static void shell_screensaver_cb(lv_timer_t* t) {
    static_cast<AppShell*>(lv_timer_get_user_data(t))->on_screensaver_tick();
}

static void bubble_to_parent(lv_obj_t* obj) {
    const uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i) {
        auto* child = lv_obj_get_child(obj, i);
        lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
        bubble_to_parent(child);
    }
}

void AppShell::on_cursor_tick() {
    if (!lbl_cursor_) return;
    cursor_visible_ = !cursor_visible_;
    lv_label_set_text(lbl_cursor_, cursor_visible_ ? "_" : " ");
}

// ── Build ─────────────────────────────────────────────────────────────────────

void AppShell::build() {
    styles::init(theme_);

    root_screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(root_screen_, styles::BG, 0);
    lv_obj_set_style_bg_opa(root_screen_, LV_OPA_COVER, 0);
    styles::make_static(root_screen_);

    build_header();
    build_content_area();
    build_tab_bar();
    build_update_overlay();
    build_screensaver();

    activity_toast_ = lv_obj_create(root_screen_);
    lv_obj_set_size(activity_toast_, 620, 34);
    lv_obj_align(activity_toast_, LV_ALIGN_BOTTOM_MID, 0, -56);
    lv_obj_set_style_bg_color(activity_toast_, styles::BG_CARD, 0);
    lv_obj_set_style_bg_opa(activity_toast_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(activity_toast_, styles::BORDER, 0);
    lv_obj_set_style_border_width(activity_toast_, 1, 0);
    lv_obj_set_style_radius(activity_toast_, 4, 0);
    lv_obj_set_style_pad_hor(activity_toast_, 10, 0);
    lv_obj_set_style_pad_ver(activity_toast_, 0, 0);
    styles::make_static(activity_toast_);
    lv_obj_add_flag(activity_toast_, LV_OBJ_FLAG_HIDDEN);

    lbl_activity_toast_ = lv_label_create(activity_toast_);
    lv_obj_add_style(lbl_activity_toast_, &styles::label_secondary, 0);
    lv_obj_set_width(lbl_activity_toast_, 596);
    lv_label_set_long_mode(lbl_activity_toast_, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl_activity_toast_, LV_ALIGN_LEFT_MID, 0, 0);

    lv_screen_load(root_screen_);

    show_tab(TAB_OVERVIEW);

    refresh_timer_ = lv_timer_create(shell_refresh_cb,  2000, this);
    clock_timer_   = lv_timer_create(shell_clock_cb,    1000, this);
    history_timer_ = lv_timer_create(shell_history_cb, 60000, this);
    stale_timer_   = lv_timer_create(shell_stale_cb,    5000, this);
    if (theme_.motion)
        cursor_timer_ = lv_timer_create(shell_cursor_cb, 600, this);
    power_timer_   = lv_timer_create(shell_power_cb,    1000, this);
    if (screensaver_timeout_s() > 0)
        screensaver_timer_ = lv_timer_create(shell_screensaver_cb, 1000, this);

    update_header_clock();
}

void AppShell::build_screensaver() {
    screensaver_ = lv_obj_create(root_screen_);
    lv_obj_set_size(screensaver_, 800, 480);
    lv_obj_set_pos(screensaver_, 0, 0);
    lv_obj_set_style_bg_color(screensaver_, styles::BG, 0);
    lv_obj_set_style_bg_opa(screensaver_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screensaver_, 0, 0);
    lv_obj_set_style_radius(screensaver_, 0, 0);
    lv_obj_set_style_pad_all(screensaver_, 0, 0);
    styles::make_static(screensaver_);
    lv_obj_add_flag(screensaver_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(screensaver_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(screensaver_, +[](lv_event_t* event) {
        auto* shell = static_cast<AppShell*>(lv_event_get_user_data(event));
        shell->hide_screensaver();
        lv_event_stop_bubbling(event);
    }, LV_EVENT_PRESSED, this);

    if (theme_.screensaver_background_enabled && !theme_.screensaver_background.empty() &&
        std::filesystem::is_regular_file(theme_.screensaver_background)) {
        screensaver_bg_gif_ = lv_gif_create(screensaver_);
        std::string path = "A:" + theme_.screensaver_background;
        lv_gif_set_src(screensaver_bg_gif_, path.c_str());
        if (lv_image_get_src(screensaver_bg_gif_)) {
            const auto* image = static_cast<const lv_image_dsc_t*>(
                lv_image_get_src(screensaver_bg_gif_));
            const uint32_t scale_x = (800U * 256U + image->header.w - 1U) / image->header.w;
            const uint32_t scale_y = (480U * 256U + image->header.h - 1U) / image->header.h;
            lv_image_set_pivot(screensaver_bg_gif_, image->header.w / 2, image->header.h / 2);
            lv_image_set_scale(screensaver_bg_gif_, std::max(scale_x, scale_y));
            lv_obj_align(screensaver_bg_gif_, LV_ALIGN_CENTER, 0, 0);
            styles::make_static(screensaver_bg_gif_);
            lv_gif_pause(screensaver_bg_gif_);

            screensaver_veil_ = lv_obj_create(screensaver_);
            lv_obj_set_size(screensaver_veil_, 800, 480);
            lv_obj_set_pos(screensaver_veil_, 0, 0);
            lv_obj_set_style_bg_color(screensaver_veil_, styles::BG, 0);
            lv_obj_set_style_bg_opa(screensaver_veil_,
                static_cast<lv_opa_t>(theme_.screensaver_veil * 255 / 100), 0);
            lv_obj_set_style_border_width(screensaver_veil_, 0, 0);
            lv_obj_set_style_radius(screensaver_veil_, 0, 0);
            styles::make_static(screensaver_veil_);
        } else {
            lv_obj_delete(screensaver_bg_gif_);
            screensaver_bg_gif_ = nullptr;
        }
    }

    auto* brand = lv_label_create(screensaver_);
    lv_obj_add_style(brand, &styles::label_value, 0);
    lv_obj_set_style_text_color(brand, styles::ACCENT, 0);
    std::string title = std::string(theme_mark_text(theme_.mark)) + "  " + theme_.title;
    lv_label_set_text(brand, title.c_str());
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 22, 18);

    lbl_saver_clock_ = lv_label_create(screensaver_);
    lv_obj_set_style_text_font(lbl_saver_clock_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_saver_clock_, styles::TEXT, 0);
    lv_obj_align(lbl_saver_clock_, LV_ALIGN_TOP_MID, 0, 70);

    lbl_saver_date_ = lv_label_create(screensaver_);
    lv_obj_add_style(lbl_saver_date_, &styles::label_value, 0);
    lv_obj_set_style_text_color(lbl_saver_date_, styles::TEXT_DIM, 0);
    lv_obj_align(lbl_saver_date_, LV_ALIGN_TOP_MID, 0, 132);

    auto make_metric = [&](int x, const char* caption, lv_obj_t** value) {
        auto* panel = lv_obj_create(screensaver_);
        lv_obj_set_size(panel, 238, 116);
        lv_obj_set_pos(panel, x, 205);
        lv_obj_add_style(panel, &styles::panel, 0);
        lv_obj_set_style_bg_opa(panel,
            static_cast<lv_opa_t>(theme_.screensaver_card_opacity * 255 / 100), 0);
        styles::make_static(panel);
        *value = lv_label_create(panel);
        lv_obj_add_style(*value, &styles::label_title, 0);
        lv_obj_set_style_text_color(*value, styles::ACCENT2, 0);
        lv_obj_align(*value, LV_ALIGN_TOP_MID, 0, 12);
        auto* label = lv_label_create(panel);
        lv_obj_add_style(label, &styles::label_secondary, 0);
        lv_label_set_text(label, caption);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -14);
    };
    make_metric(24, "NODES ONLINE", &lbl_saver_nodes_);
    make_metric(281, "RACK POWER", &lbl_saver_power_);
    make_metric(538, "RACK HEALTH", &lbl_saver_health_);

    auto* hint = lv_label_create(screensaver_);
    lv_obj_add_style(hint, &styles::label_secondary, 0);
    lv_label_set_text(hint, "Tap anywhere to return to Command Deck");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -28);
    styles::make_scroll_passthrough(screensaver_);
    bubble_to_parent(screensaver_);
    lv_obj_add_flag(screensaver_, LV_OBJ_FLAG_CLICKABLE);
}

void AppShell::show_screensaver() {
    if (!screensaver_ || screensaver_visible_) return;
    screensaver_visible_ = true;
    update_screensaver();
    lv_obj_clear_flag(screensaver_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(screensaver_);
    if (screensaver_bg_gif_) lv_gif_resume(screensaver_bg_gif_);
}

void AppShell::hide_screensaver() {
    if (!screensaver_) return;
    screensaver_visible_ = false;
    if (screensaver_bg_gif_) lv_gif_pause(screensaver_bg_gif_);
    lv_obj_add_flag(screensaver_, LV_OBJ_FLAG_HIDDEN);
    lv_display_trigger_activity(lv_display_get_default());
}

void AppShell::on_screensaver_tick() {
    if (screensaver_visible_) {
        update_screensaver();
        return;
    }
    const int timeout_s = screensaver_timeout_s();
    if (timeout_s <= 0) return;
    if (updater_.status().state == UpdateState::Running) return;
    if (lv_display_get_inactive_time(lv_display_get_default()) >=
        static_cast<uint32_t>(timeout_s) * 1000U)
        show_screensaver();
}

int AppShell::screensaver_timeout_s() const {
    return theme_.screensaver_s < 0 ? cfg_.display.screensaver_s : theme_.screensaver_s;
}

void AppShell::show_theme_studio() {
    show_tab(TAB_SETTINGS);
    if (screen_settings_) screen_settings_->show_theme_studio();
}

void AppShell::update_screensaver() {
    if (!screensaver_visible_ || !lbl_saver_clock_) return;
    time_t now = std::time(nullptr);
    struct tm* tm_info = std::localtime(&now);
    char buf[96];
    std::strftime(buf, sizeof(buf), theme_.clock_24h ? "%H:%M" : "%I:%M %p", tm_info);
    lv_label_set_text(lbl_saver_clock_, buf);
    std::strftime(buf, sizeof(buf), "%A, %B %d", tm_info);
    lv_label_set_text(lbl_saver_date_, buf);

    auto hosts = store_.snapshot();
    int online = 0, alerts = 0;
    for (const auto& [name, host] : hosts) {
        (void)name;
        if (host.online) ++online;
        else ++alerts;
        if (host.online && ((!std::isnan(host.metrics.cpu_temp_c) && host.metrics.cpu_temp_c > 75.f) ||
                            host.metrics.cpu.usage_pct > 90.f)) ++alerts;
    }
    snprintf(buf, sizeof(buf), "%d / %zu", online, hosts.size());
    lv_label_set_text(lbl_saver_nodes_, buf);
    lv_obj_set_style_text_color(lbl_saver_nodes_,
        online == static_cast<int>(hosts.size()) ? styles::OK : styles::WARN, 0);
    if (cfg_.pdu.enabled && pdu_.measurements_available())
        snprintf(buf, sizeof(buf), "~%.0f W", pdu_.total_watts());
    else
        snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(lbl_saver_power_, buf);
    const bool critical = budgets_.critical();
    const bool warning = budgets_.warning() || alerts > 0;
    lv_label_set_text(lbl_saver_health_, critical ? "CRITICAL" : warning ? "ATTENTION" : "NOMINAL");
    lv_obj_set_style_text_color(lbl_saver_health_,
        critical ? styles::DANGER : warning ? styles::WARN : styles::OK, 0);
}

void AppShell::build_update_overlay() {
    update_overlay_ = lv_obj_create(root_screen_);
    lv_obj_set_size(update_overlay_, 800, 480);
    lv_obj_set_pos(update_overlay_, 0, 0);
    lv_obj_set_style_bg_color(update_overlay_, styles::BG, 0);
    lv_obj_set_style_bg_opa(update_overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(update_overlay_, 0, 0);
    lv_obj_set_style_radius(update_overlay_, 0, 0);
    styles::make_static(update_overlay_);
    lv_obj_add_flag(update_overlay_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* panel = lv_obj_create(update_overlay_);
    lv_obj_set_size(panel, 620, 220);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(panel, &styles::panel, 0);
    lv_obj_set_style_border_color(panel, styles::ACCENT, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    styles::make_static(panel);

    lv_obj_t* title = lv_label_create(panel);
    lv_obj_add_style(title, &styles::label_title, 0);
    lv_obj_set_style_text_color(title, styles::ACCENT, 0);
    lv_label_set_text(title, "COMMAND DECK UPDATE");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lbl_update_overlay_status_ = lv_label_create(panel);
    lv_obj_add_style(lbl_update_overlay_status_, &styles::label_value, 0);
    lv_obj_set_style_text_color(lbl_update_overlay_status_, styles::TEXT, 0);
    lv_obj_set_width(lbl_update_overlay_status_, 560);
    lv_obj_set_style_text_align(lbl_update_overlay_status_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_update_overlay_status_, "Update installed.\nRestarting dashboard...");
    lv_obj_align(lbl_update_overlay_status_, LV_ALIGN_CENTER, 0, 20);

    lv_obj_t* bar = lv_bar_create(panel);
    lv_obj_set_size(bar, 520, 10);
    lv_obj_add_style(bar, &styles::bar_track, LV_PART_MAIN);
    lv_obj_add_style(bar, &styles::bar_cpu_ind, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 100, LV_ANIM_OFF);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -28);
}

void AppShell::build_header() {
    header_bar_ = lv_obj_create(root_screen_);
    lv_obj_set_size(header_bar_, 800, 40);
    lv_obj_set_pos(header_bar_, 0, 0);
    lv_obj_add_style(header_bar_, &styles::header_bar, 0);
    styles::make_static(header_bar_);

    // ── Normal-mode widgets ───────────────────────────────────────────────────

    lbl_title_ = lv_label_create(header_bar_);
    lv_obj_add_style(lbl_title_, &styles::label_value, 0);
    lv_obj_set_style_text_font(lbl_title_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_title_, styles::ACCENT, 0);
    lv_obj_align(lbl_title_, LV_ALIGN_LEFT_MID, 8, 0);
    int title_x = 8;
    if (const auto* mark = theme_mark_image(theme_.mark)) {
        auto* image = lv_image_create(header_bar_);
        lv_image_set_src(image, mark);
        lv_obj_align(image, LV_ALIGN_LEFT_MID, 8, 0);
        styles::make_static(image);
        title_x = 48;
    }
    std::string title = theme_mark_image(theme_.mark) ? theme_.title :
                        std::string(theme_mark_text(theme_.mark)) + "  " + theme_.title;
    if (!theme_.subtitle.empty()) title += " // " + theme_.subtitle;
    lv_label_set_text(lbl_title_, title.c_str());
    lv_obj_align(lbl_title_, LV_ALIGN_LEFT_MID, title_x, 0);

    lbl_cursor_ = lv_label_create(header_bar_);
    lv_obj_add_style(lbl_cursor_, &styles::label_value, 0);
    lv_obj_set_style_text_color(lbl_cursor_, styles::ACCENT, 0);
    lv_obj_align_to(lbl_cursor_, lbl_title_, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
    lv_label_set_text(lbl_cursor_, "_");

    lbl_badge_ = lv_label_create(header_bar_);
    lv_obj_add_style(lbl_badge_, &styles::label_secondary, 0);
    lv_obj_align(lbl_badge_, LV_ALIGN_RIGHT_MID, -88, 0);
    lv_label_set_text(lbl_badge_, "");

    // ── Detail-mode widgets (hidden until entering detail) ────────────────────

    btn_header_back_ = lv_button_create(header_bar_);
    lv_obj_add_style(btn_header_back_, &styles::btn_action, 0);
    lv_obj_add_style(btn_header_back_, &styles::btn_action_pressed, LV_STATE_PRESSED);
    lv_obj_set_height(btn_header_back_, 30);
    lv_obj_set_style_shadow_width(btn_header_back_, 0, 0);
    styles::make_static(btn_header_back_);
    lv_obj_align(btn_header_back_, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_add_flag(btn_header_back_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* back_lbl = lv_label_create(btn_header_back_);
    lv_label_set_text(back_lbl, "< Back");

    lv_obj_add_event_cb(btn_header_back_, +[](lv_event_t* e) {
        static_cast<AppShell*>(lv_event_get_user_data(e))->pop_host_detail();
    }, LV_EVENT_CLICKED, this);

    lbl_detail_host_ = lv_label_create(header_bar_);
    lv_obj_add_style(lbl_detail_host_, &styles::label_value, 0);
    lv_obj_set_style_text_color(lbl_detail_host_, styles::TEXT, 0);
    lv_obj_align(lbl_detail_host_, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(lbl_detail_host_, "");
    lv_obj_add_flag(lbl_detail_host_, LV_OBJ_FLAG_HIDDEN);

    lbl_detail_status_ = lv_label_create(header_bar_);
    lv_obj_add_style(lbl_detail_status_, &styles::label_secondary, 0);
    lv_obj_align(lbl_detail_status_, LV_ALIGN_RIGHT_MID, -88, 0);
    lv_label_set_text(lbl_detail_status_, "");
    lv_obj_add_flag(lbl_detail_status_, LV_OBJ_FLAG_HIDDEN);

    // Clock — always visible
    lbl_clock_ = lv_label_create(header_bar_);
    lv_obj_add_style(lbl_clock_, &styles::label_secondary, 0);
    lv_obj_align(lbl_clock_, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_label_set_text(lbl_clock_, "00:00:00");
}

void AppShell::build_content_area() {
    content_area_ = lv_obj_create(root_screen_);
    lv_obj_set_size(content_area_, 800, 390);
    lv_obj_set_pos(content_area_, 0, 40);
    lv_obj_set_style_bg_color(content_area_, styles::BG, 0);
    lv_obj_set_style_bg_opa(content_area_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content_area_, 0, 0);
    lv_obj_set_style_pad_all(content_area_, 0, 0);
    lv_obj_set_style_radius(content_area_, 0, 0);
    styles::make_static(content_area_);
}

void AppShell::build_tab_bar() {
    tab_bar_ = lv_obj_create(root_screen_);
    lv_obj_set_size(tab_bar_, 800, 50);
    lv_obj_set_pos(tab_bar_, 0, 430);
    lv_obj_add_style(tab_bar_, &styles::tab_bar, 0);
    lv_obj_set_flex_flow(tab_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_bar_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tab_bar_, 0, 0);
    lv_obj_set_style_pad_gap(tab_bar_, 0, 0);
    styles::make_static(tab_bar_);

    for (int i = 0; i < TAB_COUNT; ++i) {
        lv_obj_t* btn = lv_button_create(tab_bar_);
        lv_obj_set_size(btn, 200, 50);
        lv_obj_add_style(btn, &styles::tab_btn_inactive, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        styles::make_static(btn);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, TAB_LABELS[i]);
        lv_obj_center(lbl);

        struct TabCbData { AppShell* shell; int tab; };
        auto* d = new TabCbData{ this, i };
        lv_obj_add_event_cb(btn, +[](lv_event_t* e) {
            auto* d = static_cast<TabCbData*>(lv_event_get_user_data(e));
            d->shell->show_tab((Tab)d->tab);
        }, LV_EVENT_CLICKED, d);

        tab_btns_[i] = btn;
    }
}

// ── Detail-mode header swap ───────────────────────────────────────────────────

void AppShell::enter_detail_mode(const std::string& host, bool online) {
    detail_host_ = host;

    // Hide normal-mode widgets
    if (lbl_title_)  lv_obj_add_flag(lbl_title_,  LV_OBJ_FLAG_HIDDEN);
    if (lbl_cursor_) lv_obj_add_flag(lbl_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (lbl_badge_)  lv_obj_add_flag(lbl_badge_,  LV_OBJ_FLAG_HIDDEN);

    // Show detail-mode widgets
    if (btn_header_back_)   lv_obj_clear_flag(btn_header_back_,   LV_OBJ_FLAG_HIDDEN);
    if (lbl_detail_host_) {
        lv_obj_clear_flag(lbl_detail_host_, LV_OBJ_FLAG_HIDDEN);
        // Use display_name if set
        auto e = store_.get(host);
        const std::string& disp = (e && !e->display_name.empty()) ? e->display_name : host;
        lv_label_set_text(lbl_detail_host_, disp.c_str());
    }
    if (lbl_detail_status_) {
        lv_obj_clear_flag(lbl_detail_status_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_detail_status_, online ? "* ONLINE" : "* OFFLINE");
        lv_obj_set_style_text_color(lbl_detail_status_,
            online ? styles::OK : styles::TEXT_DIM, 0);
    }
}

void AppShell::exit_detail_mode() {
    detail_host_.clear();

    // Restore normal-mode widgets
    if (lbl_title_)  lv_obj_clear_flag(lbl_title_,  LV_OBJ_FLAG_HIDDEN);
    if (lbl_cursor_) lv_obj_clear_flag(lbl_cursor_, LV_OBJ_FLAG_HIDDEN);
    if (lbl_badge_)  lv_obj_clear_flag(lbl_badge_,  LV_OBJ_FLAG_HIDDEN);

    // Hide detail-mode widgets
    if (btn_header_back_)   lv_obj_add_flag(btn_header_back_,   LV_OBJ_FLAG_HIDDEN);
    if (lbl_detail_host_)   lv_obj_add_flag(lbl_detail_host_,   LV_OBJ_FLAG_HIDDEN);
    if (lbl_detail_status_) lv_obj_add_flag(lbl_detail_status_, LV_OBJ_FLAG_HIDDEN);
}

// ── Navigation ────────────────────────────────────────────────────────────────

void AppShell::show_tab(Tab t) {
    if (detail_visible_) {
        detail_visible_ = false;
        if (screen_detail_) screen_detail_->hide();
        exit_detail_mode();
    }

    for (int i = 0; i < TAB_COUNT; ++i) {
        if (!tab_btns_[i]) continue;
        lv_obj_remove_style(tab_btns_[i], &styles::tab_btn_active, 0);
        lv_obj_remove_style(tab_btns_[i], &styles::tab_btn_inactive, 0);
        lv_obj_add_style(tab_btns_[i],
            i == t ? &styles::tab_btn_active : &styles::tab_btn_inactive, 0);
        lv_obj_refresh_style(tab_btns_[i], LV_PART_MAIN, LV_STYLE_PROP_ANY);
    }

    auto hide_all = [&] {
        if (screen_overview_) screen_overview_->hide();
        if (screen_hosts_)    screen_hosts_->hide();
        if (screen_pdu_)      screen_pdu_->hide();
        if (screen_settings_) screen_settings_->hide();
    };

    hide_all();

    switch (t) {
    case TAB_OVERVIEW:
        if (!screen_overview_)
            screen_overview_ = std::make_unique<ScreenOverview>(content_area_,
                [this](const std::string& h) { show_host_detail(h); });
        screen_overview_->show();
        break;
    case TAB_HOSTS:
        if (!screen_hosts_)
            screen_hosts_ = std::make_unique<ScreenHosts>(content_area_,
                [this](const std::string& h) { show_host_detail(h); });
        screen_hosts_->show();
        break;
    case TAB_PDU:
        if (!screen_pdu_)
            screen_pdu_ = std::make_unique<ScreenPdu>(content_area_, pdu_,
                [this](int outlet, bool new_state) {
                    protocol::CommandMessage cmd;
                    cmd.id           = "pdu_toggle";
                    cmd.action       = "outlet";
                    cmd.outlet_num   = outlet;
                    cmd.outlet_state = new_state;
                    router_.dispatch(cmd, "", outlet);
                }, cfg_.pdu.enabled, cfg_, power_history_, sequences_, budgets_);
        screen_pdu_->show();
        break;
    case TAB_SETTINGS:
        if (!screen_settings_)
            screen_settings_ = std::make_unique<ScreenSettings>(
                content_area_, cfg_, activity_, updater_, themes_,
                [this](const ThemeProfile& profile) { apply_theme(profile); });
        screen_settings_->show();
        break;
    default: break;
    }

    active_tab_ = t;
    refresh();
}

void AppShell::show_host_detail(const std::string& host) {
    if (!screen_detail_)
        screen_detail_ = std::make_unique<ScreenHostDetail>(
            content_area_, store_, pdu_, router_,
            activity_,
            [this] { pop_host_detail(); });

    if (screen_overview_) screen_overview_->hide();
    if (screen_hosts_)    screen_hosts_->hide();
    if (screen_pdu_)      screen_pdu_->hide();
    if (screen_settings_) screen_settings_->hide();

    auto e = store_.get(host);
    enter_detail_mode(host, e ? e->online : false);

    screen_detail_->show(host);
    detail_visible_ = true;
}

void AppShell::pop_host_detail() {
    detail_visible_ = false;
    if (screen_detail_) screen_detail_->hide();
    exit_detail_mode();
    show_tab(active_tab_);
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void AppShell::refresh() {
    auto hosts   = store_.snapshot();
    auto outlets = pdu_.get();
    update_activity_toast();
    update_update_overlay();
    update_screensaver();

    if (detail_visible_ && screen_detail_) {
        // Keep detail header status current
        if (!detail_host_.empty()) {
            auto e = store_.get(detail_host_);
            if (e) enter_detail_mode(detail_host_, e->online);
        }
        screen_detail_->refresh();
        return;
    }

    update_alert_badge();

    switch (active_tab_) {
    case TAB_OVERVIEW:
        if (screen_overview_) screen_overview_->refresh(hosts, outlets, cfg_.pdu.enabled,
                                                        pdu_.measurements_available(),
                                                        pdu_.total_watts(),
                                                        budgets_.warning(), budgets_.critical());
        break;
    case TAB_HOSTS:
        if (screen_hosts_) screen_hosts_->refresh(hosts, outlets, cfg_.pdu.enabled);
        break;
    case TAB_PDU:
        if (screen_pdu_) screen_pdu_->refresh(outlets);
        break;
    case TAB_SETTINGS:
        if (screen_settings_) screen_settings_->refresh(hosts);
        break;
    default: break;
    }
}

void AppShell::update_update_overlay() {
    if (!update_overlay_ || !lbl_update_overlay_status_) return;

    auto update = updater_.status();
    if (update.state != UpdateState::Succeeded &&
        !(update.state == UpdateState::Running && update.progress >= 100)) return;

    lv_label_set_text(lbl_update_overlay_status_, "Update installed.\nRestarting dashboard...");
    lv_obj_clear_flag(update_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(update_overlay_);
}

void AppShell::update_activity_toast() {
    if (!activity_toast_ || !lbl_activity_toast_) return;
    auto recent = activity_.snapshot();
    auto it = std::max_element(recent.begin(), recent.end(), [](const CommandActivity& a,
                                                               const CommandActivity& b) {
        return a.completed_at < b.completed_at;
    });
    if (it != recent.end() && it->completed_at > 0 && it->id != last_toast_id_) {
        const auto& a = *it;
        last_toast_id_ = a.id;
        toast_ticks_ = 5;
        std::string text = a.state == ActivityState::Succeeded ? "OK  " : "FAILED  ";
        if (!a.host.empty()) text += a.host + "  ";
        text += a.action;
        if (!a.output.empty()) {
            std::string output = a.output.substr(0, 120);
            for (char& c : output) if (c == '\n' || c == '\r') c = ' ';
            text += "  " + output;
        }
        lv_label_set_text(lbl_activity_toast_, text.c_str());
        lv_obj_set_style_text_color(lbl_activity_toast_,
            a.state == ActivityState::Succeeded ? styles::OK : styles::DANGER, 0);
        lv_obj_clear_flag(activity_toast_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(activity_toast_);
    } else if (toast_ticks_ > 0 && --toast_ticks_ == 0) {
        lv_obj_add_flag(activity_toast_, LV_OBJ_FLAG_HIDDEN);
    }
}

void AppShell::update_header_clock() {
    if (!lbl_clock_) return;
    time_t now = std::time(nullptr);
    struct tm* tm_info = std::localtime(&now);
    char buf[16];
    std::strftime(buf, sizeof(buf), theme_.clock_24h ? "%H:%M:%S" : "%I:%M %p", tm_info);
    lv_label_set_text(lbl_clock_, buf);
}

void AppShell::apply_theme(const ThemeProfile& profile) {
    std::string error;
    if (!themes_.set_active(profile, &error)) return;
    theme_ = profile;
    auto* data = new AppShell*(this);
    lv_async_call(+[](void* value) {
        auto* holder = static_cast<AppShell**>(value);
        (*holder)->rebuild_ui();
        delete holder;
    }, data);
}

void AppShell::rebuild_ui() {
    if (refresh_timer_) lv_timer_del(refresh_timer_);
    if (clock_timer_) lv_timer_del(clock_timer_);
    if (history_timer_) lv_timer_del(history_timer_);
    if (stale_timer_) lv_timer_del(stale_timer_);
    if (cursor_timer_) lv_timer_del(cursor_timer_);
    if (power_timer_) lv_timer_del(power_timer_);
    if (screensaver_timer_) lv_timer_del(screensaver_timer_);
    refresh_timer_ = clock_timer_ = history_timer_ = stale_timer_ = cursor_timer_ = power_timer_ = nullptr;
    screensaver_timer_ = nullptr;

    lv_obj_t* old_root = root_screen_;
    screen_detail_.reset();
    screen_settings_.reset();
    screen_pdu_.reset();
    screen_hosts_.reset();
    screen_overview_.reset();
    root_screen_ = header_bar_ = lbl_title_ = lbl_clock_ = lbl_badge_ = content_area_ = nullptr;
    tab_bar_ = activity_toast_ = lbl_activity_toast_ = update_overlay_ = screensaver_ = nullptr;
    screensaver_bg_gif_ = screensaver_veil_ = nullptr;
    lbl_saver_clock_ = lbl_saver_date_ = lbl_saver_nodes_ = lbl_saver_power_ = lbl_saver_health_ = nullptr;
    lbl_update_overlay_status_ = btn_header_back_ = lbl_detail_host_ = lbl_detail_status_ = nullptr;
    for (auto& button : tab_btns_) button = nullptr;
    detail_visible_ = false;
    screensaver_visible_ = false;
    styles::apply(theme_);
    build();
    if (old_root) lv_obj_delete(old_root);
}

void AppShell::update_alert_badge() {
    if (!lbl_badge_) return;
    auto hosts = store_.snapshot();
    int offline = 0, danger = 0;
    for (auto& [name, e] : hosts) {
        if (!e.online) { ++offline; continue; }
        if (!std::isnan(e.metrics.cpu_temp_c) && e.metrics.cpu_temp_c > 75.f) ++danger;
        if (e.metrics.cpu.usage_pct > 90.f) ++danger;
    }
    char buf[32];
    if (offline > 0 || danger > 0 || budgets_.warning()) {
        if (budgets_.critical()) snprintf(buf, sizeof(buf), "! POWER CRITICAL");
        else if (budgets_.warning()) snprintf(buf, sizeof(buf), "! POWER WARNING");
        else snprintf(buf, sizeof(buf), "! %d offline  %d hot", offline, danger);
        lv_obj_set_style_text_color(lbl_badge_, styles::WARN, 0);
    } else {
        snprintf(buf, sizeof(buf), "ALL OK");
        lv_obj_set_style_text_color(lbl_badge_, styles::OK, 0);
    }
    lv_label_set_text(lbl_badge_, buf);
}
