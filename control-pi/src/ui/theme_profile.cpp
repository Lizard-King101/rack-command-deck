#include "theme_profile.h"
#include "compiled_marks.generated.h"
#include <lvgl.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

namespace {
uint32_t color_value(const toml::table& table, const char* key, uint32_t fallback) {
    auto value = table[key].value<std::string>();
    if (!value) return fallback;
    std::string text = *value;
    if (!text.empty() && text[0] == '#') text.erase(0, 1);
    if (text.size() != 6) return fallback;
    try {
        return static_cast<uint32_t>(std::stoul(text, nullptr, 16)) & 0xffffff;
    } catch (...) {
        return fallback;
    }
}

std::string color_text(uint32_t color) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%06X", color & 0xffffff);
    return buf;
}

ThemeProfile restrained() {
    ThemeProfile p;
    p.id = "graphite";
    p.name = "Graphite";
    p.mark = "rack";
    p.bg = 0x0d1117; p.bg_card = 0x161b22; p.bg_raised = 0x21262d; p.bg_header = 0x090c10;
    p.accent = 0x58a6ff; p.accent2 = 0x79c0ff; p.accent_dim = 0x1f4b7a;
    p.ok = 0x3fb950; p.warn = 0xd29922; p.danger = 0xf85149;
    p.text = 0xf0f6fc; p.text_dim = 0x8b949e; p.border = 0x30363d; p.bar_track = 0x21262d;
    p.radius = 4; p.shadow = 0;
    return p;
}

ThemeProfile light() {
    ThemeProfile p;
    p.id = "daylight";
    p.name = "Daylight";
    p.mark = "grid";
    p.bg = 0xf2f5f8; p.bg_card = 0xffffff; p.bg_raised = 0xe7edf3; p.bg_header = 0xdfe7ef;
    p.accent = 0x6542c7; p.accent2 = 0x006f8a; p.accent_dim = 0xcfc3f2;
    p.ok = 0x087f5b; p.warn = 0xa15c00; p.danger = 0xc92a2a;
    p.text = 0x17202a; p.text_dim = 0x52606d; p.border = 0xb9c4cf; p.bar_track = 0xd5dde5;
    p.radius = 8; p.shadow = 5;
    return p;
}

ThemeProfile contrast() {
    ThemeProfile p;
    p.id = "high-contrast";
    p.name = "High Contrast";
    p.mark = "terminal";
    p.bg = 0x000000; p.bg_card = 0x050505; p.bg_raised = 0x111111; p.bg_header = 0x000000;
    p.accent = 0xffff00; p.accent2 = 0x00ffff; p.accent_dim = 0x4d4d00;
    p.ok = 0x00ff00; p.warn = 0xffff00; p.danger = 0xff3030;
    p.text = 0xffffff; p.text_dim = 0xc8c8c8; p.border = 0xffffff; p.bar_track = 0x333333;
    p.radius = 0; p.shadow = 0; p.motion = 0;
    return p;
}
}

ThemeStore::ThemeStore(std::string directory) {
    if (directory.empty()) {
        const char* home = std::getenv("HOME");
        directory = std::string(home ? home : ".") + "/.config/command-deck/profiles";
    }
    directory_ = std::move(directory);
}

std::vector<ThemeProfile> ThemeStore::presets() {
    return {ThemeProfile{}, restrained(), light(), contrast()};
}

ThemeProfile ThemeStore::preset(const std::string& id) {
    for (const auto& profile : presets())
        if (profile.id == id) return profile;
    return ThemeProfile{};
}

std::string ThemeStore::sanitize_id(const std::string& value) {
    std::string result;
    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c))) result +=
            static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if ((c == '-' || c == '_' || c == ' ') && !result.empty() && result.back() != '-')
            result += '-';
    }
    while (!result.empty() && result.back() == '-') result.pop_back();
    return result.empty() ? "custom" : result;
}

ThemeProfile ThemeStore::load_file(const std::string& path, const ThemeProfile& fallback) {
    try {
        auto root = toml::parse_file(path);
        ThemeProfile p = fallback;
        p.id = sanitize_id(root["profile"]["id"].value_or(p.id));
        p.name = root["profile"]["name"].value_or(p.name);
        p.title = root["branding"]["title"].value_or(p.title);
        p.subtitle = root["branding"]["subtitle"].value_or(p.subtitle);
        p.mark = root["branding"]["mark"].value_or(p.mark);
        p.density = std::clamp((int)root["appearance"]["density"].value_or((int64_t)p.density), -1, 1);
        p.radius = std::clamp((int)root["appearance"]["radius"].value_or((int64_t)p.radius), 0, 16);
        p.shadow = std::clamp((int)root["appearance"]["shadow"].value_or((int64_t)p.shadow), 0, 20);
        p.motion = std::clamp((int)root["appearance"]["motion"].value_or((int64_t)p.motion), 0, 1);
        p.clock_24h = root["appearance"]["clock_24h"].value_or(p.clock_24h);
        p.screensaver_s = std::clamp(
            (int)root["appearance"]["screensaver_s"].value_or((int64_t)p.screensaver_s), -1, 3600);
        p.screensaver_background_enabled = root["screensaver"]["background_enabled"].value_or(
            p.screensaver_background_enabled);
        p.screensaver_background = root["screensaver"]["background"].value_or(
            p.screensaver_background);
        p.screensaver_veil = std::clamp(
            (int)root["screensaver"]["veil"].value_or((int64_t)p.screensaver_veil), 0, 100);
        p.screensaver_card_opacity = std::clamp(
            (int)root["screensaver"]["card_opacity"].value_or(
                (int64_t)p.screensaver_card_opacity), 0, 100);
        if (auto colors = root["colors"].as_table()) {
#define LOAD_COLOR(name) p.name = color_value(*colors, #name, p.name)
            LOAD_COLOR(bg); LOAD_COLOR(bg_card); LOAD_COLOR(bg_raised); LOAD_COLOR(bg_header);
            LOAD_COLOR(accent); LOAD_COLOR(accent2); LOAD_COLOR(accent_dim); LOAD_COLOR(ok);
            LOAD_COLOR(warn); LOAD_COLOR(danger); LOAD_COLOR(text); LOAD_COLOR(text_dim);
            LOAD_COLOR(border); LOAD_COLOR(bar_track);
#undef LOAD_COLOR
        }
        auto marks = theme_mark_ids();
        if (std::find(marks.begin(), marks.end(), p.mark) == marks.end())
            p.mark = "terminal";
        return p;
    } catch (...) {
        return fallback;
    }
}

bool ThemeStore::write_file(const std::string& path, const ThemeProfile& p, std::string* error) {
    try {
        fs::create_directories(fs::path(path).parent_path());
        toml::table profile;
        profile.insert("id", sanitize_id(p.id));
        profile.insert("name", p.name);
        toml::table branding;
        branding.insert("title", p.title);
        branding.insert("subtitle", p.subtitle);
        branding.insert("mark", p.mark);
        toml::table appearance;
        appearance.insert("density", p.density);
        appearance.insert("radius", p.radius);
        appearance.insert("shadow", p.shadow);
        appearance.insert("motion", p.motion);
        appearance.insert("clock_24h", p.clock_24h);
        appearance.insert("screensaver_s", p.screensaver_s);
        toml::table screensaver;
        screensaver.insert("background_enabled", p.screensaver_background_enabled);
        screensaver.insert("background", p.screensaver_background);
        screensaver.insert("veil", p.screensaver_veil);
        screensaver.insert("card_opacity", p.screensaver_card_opacity);
        toml::table colors;
#define INSERT_COLOR(name) colors.insert(#name, color_text(p.name))
        INSERT_COLOR(bg); INSERT_COLOR(bg_card); INSERT_COLOR(bg_raised); INSERT_COLOR(bg_header);
        INSERT_COLOR(accent); INSERT_COLOR(accent2); INSERT_COLOR(accent_dim); INSERT_COLOR(ok);
        INSERT_COLOR(warn); INSERT_COLOR(danger); INSERT_COLOR(text); INSERT_COLOR(text_dim);
        INSERT_COLOR(border); INSERT_COLOR(bar_track);
#undef INSERT_COLOR
        toml::table root;
        root.insert("profile", std::move(profile));
        root.insert("branding", std::move(branding));
        root.insert("appearance", std::move(appearance));
        root.insert("screensaver", std::move(screensaver));
        root.insert("colors", std::move(colors));
        std::ofstream out(path);
        out << root;
        if (!out.good()) throw std::runtime_error("write failed");
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

std::vector<ThemeProfile> ThemeStore::profiles() const {
    auto result = presets();
    std::error_code ec;
    if (!fs::exists(directory_, ec)) return result;
    for (const auto& entry : fs::directory_iterator(directory_, ec)) {
        if (entry.path().extension() != ".toml") continue;
        auto loaded = load_file(entry.path().string(), ThemeProfile{});
        auto it = std::find_if(result.begin(), result.end(), [&](const auto& p) { return p.id == loaded.id; });
        if (it == result.end()) result.push_back(std::move(loaded));
        else *it = std::move(loaded);
    }
    return result;
}

std::string ThemeStore::active_id() const {
    if (const char* override_id = std::getenv("DECK_THEME"))
        return sanitize_id(override_id);
    std::ifstream in(fs::path(directory_) / ".active");
    std::string id;
    std::getline(in, id);
    return sanitize_id(id.empty() ? "neon" : id);
}

ThemeProfile ThemeStore::active() const {
    const auto id = active_id();
    for (const auto& p : profiles()) if (p.id == id) return p;
    return ThemeProfile{};
}

bool ThemeStore::save(const ThemeProfile& profile, std::string* error) const {
    return write_file((fs::path(directory_) / (sanitize_id(profile.id) + ".toml")).string(),
                      profile, error);
}

bool ThemeStore::set_active(const ThemeProfile& profile, std::string* error) const {
    if (!save(profile, error)) return false;
    try {
        fs::create_directories(directory_);
        std::ofstream out(fs::path(directory_) / ".active");
        out << sanitize_id(profile.id) << "\n";
        if (!out.good()) throw std::runtime_error("write failed");
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

bool ThemeStore::remove(const std::string& id, std::string* error) const {
    std::error_code ec;
    fs::remove(fs::path(directory_) / (sanitize_id(id) + ".toml"), ec);
    if (ec && error) *error = ec.message();
    return !ec;
}

bool ThemeStore::export_profile(const ThemeProfile& profile, std::string* path,
                                std::string* error) const {
    fs::path target = fs::path(directory_).parent_path() / "exports" /
                      (sanitize_id(profile.id) + ".toml");
    if (path) *path = target.string();
    return write_file(target.string(), profile, error);
}

const char* theme_mark_text(const std::string& id) {
    if (id == "rack") return "[=]";
    if (id == "grid") return "[#]";
    if (id == "pulse") return "/\\/\\";
    return ">_";
}

const lv_image_dsc_t* theme_mark_image(const std::string& id) {
#define FIND_MARK(mark_id, symbol) if (id == mark_id) return &symbol;
    COMMAND_DECK_CUSTOM_MARKS(FIND_MARK)
#undef FIND_MARK
    return nullptr;
}

std::vector<std::string> theme_mark_ids() {
    std::vector<std::string> result = {"terminal", "rack", "grid", "pulse"};
#define ADD_MARK(mark_id, symbol) result.push_back(mark_id);
    COMMAND_DECK_CUSTOM_MARKS(ADD_MARK)
#undef ADD_MARK
    return result;
}
