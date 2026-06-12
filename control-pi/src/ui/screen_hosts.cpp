#include "screen_hosts.h"
#include "host_card_wide.h"
#include "styles.h"

ScreenHosts::ScreenHosts(lv_obj_t* parent, DetailCb on_detail)
    : on_detail_(std::move(on_detail)) {
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, 800, 390);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, styles::BG, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    styles::make_static(container_);

    lv_obj_t* title = lv_label_create(container_);
    lv_obj_add_style(title, &styles::label_value, 0);
    lv_obj_set_style_text_color(title, styles::TEXT, 0);
    lv_obj_set_pos(title, 12, 8);
    lv_label_set_text(title, "FLEET");

    lv_obj_t* hint = lv_label_create(container_);
    lv_obj_add_style(hint, &styles::label_secondary, 0);
    lv_obj_set_pos(hint, 12, 29);
    lv_label_set_text(hint, "All known nodes, live metrics, and power state");

    scroll_cont_ = lv_obj_create(container_);
    lv_obj_set_size(scroll_cont_, 800, 342);
    lv_obj_set_pos(scroll_cont_, 0, 48);
    lv_obj_set_flex_flow(scroll_cont_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(scroll_cont_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_cont_, 0, 0);
    lv_obj_set_style_pad_all(scroll_cont_, 8, 0);
    lv_obj_set_style_pad_row(scroll_cont_, 6, 0);
    styles::make_vertical_scroll(scroll_cont_);
}

void ScreenHosts::refresh(const std::map<std::string, HostEntry>& hosts,
                           const std::vector<protocol::OutletState>& outlets,
                           bool pdu_enabled) {
    // Remove cards for hosts no longer in store
    uint32_t n = lv_obj_get_child_count(scroll_cont_);
    for (int32_t i = (int32_t)n - 1; i >= 0; --i) {
        lv_obj_t* card = lv_obj_get_child(scroll_cont_, i);
        auto* s = static_cast<std::string*>(lv_obj_get_user_data(card));
        if (s && hosts.find(*s) == hosts.end()) lv_obj_del(card);
    }

    for (auto& [name, entry] : hosts) {
        std::optional<protocol::OutletState> outlet;
        int eff = entry.effective_pdu_outlet();
        if (pdu_enabled && eff > 0) {
            for (auto& o : outlets)
                if (o.outlet == eff) { outlet = o; break; }
        }
        HostCardWide::update_or_create(scroll_cont_, name, entry, outlet, pdu_enabled,
            [this](const std::string& h) { if (on_detail_) on_detail_(h); });
    }
}
