#include "screen_settings.h"
#include "styles.h"
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

ScreenSettings::ScreenSettings(lv_obj_t* parent, const Config& cfg, ActivityStore& activity,
                               UpdateManager& updater)
    : cfg_(cfg), activity_(activity), updater_(updater) {
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, 800, 390);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, styles::BG, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    styles::make_static(container_);

    lv_obj_t* scroll = lv_obj_create(container_);
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
        lv_bar_set_value(bar_update_, update.progress, LV_ANIM_ON);
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
