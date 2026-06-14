#pragma once

#include <cstdint>
#include <lvgl.h>
#include <string>
#include <vector>

struct ThemeProfile {
    std::string id = "neon";
    std::string name = "Command Deck Neon";
    std::string title = "COMMAND DECK";
    std::string subtitle = "RACK OPS";
    std::string mark = "terminal";

    uint32_t bg = 0x08080e;
    uint32_t bg_card = 0x141020;
    uint32_t bg_raised = 0x1a152a;
    uint32_t bg_header = 0x0d0a1a;
    uint32_t accent = 0x8844ff;
    uint32_t accent2 = 0x00e5ff;
    uint32_t accent_dim = 0x442288;
    uint32_t ok = 0x00ff88;
    uint32_t warn = 0xffaa00;
    uint32_t danger = 0xff2244;
    uint32_t text = 0xe8e0ff;
    uint32_t text_dim = 0x605580;
    uint32_t border = 0x2a2044;
    uint32_t bar_track = 0x221838;

    int density = 0;       // -1 compact, 0 standard, 1 comfortable
    int radius = 6;
    int shadow = 8;
    int motion = 1;        // 0 reduced, 1 standard
    bool clock_24h = true;
    int screensaver_s = -1; // -1 uses system config, 0 disables
    bool screensaver_background_enabled = true;
    std::string screensaver_background;
    int screensaver_veil = 55; // percentage of theme background over the animation
    int screensaver_card_opacity = 90; // percentage opacity of rack-status cards
};

class ThemeStore {
public:
    explicit ThemeStore(std::string directory = {});

    const std::string& directory() const { return directory_; }
    std::vector<ThemeProfile> profiles() const;
    ThemeProfile active() const;
    bool save(const ThemeProfile& profile, std::string* error = nullptr) const;
    bool set_active(const ThemeProfile& profile, std::string* error = nullptr) const;
    bool remove(const std::string& id, std::string* error = nullptr) const;
    bool export_profile(const ThemeProfile& profile, std::string* path = nullptr,
                        std::string* error = nullptr) const;

    static ThemeProfile preset(const std::string& id);
    static std::vector<ThemeProfile> presets();
    static std::string sanitize_id(const std::string& value);

private:
    static ThemeProfile load_file(const std::string& path, const ThemeProfile& fallback);
    static bool write_file(const std::string& path, const ThemeProfile& profile,
                           std::string* error);
    std::string active_id() const;

    std::string directory_;
};

const char* theme_mark_text(const std::string& id);
const lv_image_dsc_t* theme_mark_image(const std::string& id);
std::vector<std::string> theme_mark_ids();
