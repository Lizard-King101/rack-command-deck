#include "ui/theme_profile.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    const std::string dir = "/tmp/command-deck-theme-test/profiles";
    std::filesystem::remove_all("/tmp/command-deck-theme-test");
    ThemeStore store(dir);

    auto presets = store.profiles();
    assert(presets.size() == 4);
    assert(store.active().id == "neon");

    auto profile = ThemeStore::preset("daylight");
    profile.id = "My Custom Theme";
    profile.name = "My Custom Theme";
    profile.title = "Nick's \"Rack\"";
    profile.subtitle = "Lab\nDeck";
    profile.screensaver_s = 300;
    profile.screensaver_background = "/tmp/background.gif";
    profile.screensaver_veil = 70;
    profile.screensaver_card_opacity = 65;
    profile.radius = 99;
    profile.mark = "unknown";
    assert(store.set_active(profile));
    assert(store.active().id == "my-custom-theme");
    assert(store.active().radius == 16);
    assert(store.active().mark == "terminal");
    assert(store.active().title == profile.title);
    assert(store.active().subtitle == profile.subtitle);
    assert(store.active().screensaver_s == 300);
    assert(store.active().screensaver_background == profile.screensaver_background);
    assert(store.active().screensaver_veil == 70);
    assert(store.active().screensaver_card_opacity == 65);
    assert(theme_mark_text("rack") == std::string("[=]"));
    assert(theme_mark_image("rack") == nullptr);

    std::string export_path;
    assert(store.export_profile(store.active(), &export_path));
    assert(std::filesystem::exists(export_path));

    std::ofstream malformed(std::filesystem::path(dir) / "broken.toml");
    malformed << "not valid toml = [";
    malformed.close();
    assert(store.profiles().size() >= 5);
    return 0;
}
