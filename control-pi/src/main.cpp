#include "config.h"
#include "metrics_store.h"
#include "activity_store.h"
#include "command_router.h"
#include "ws_server.h"
#include "pdu/synlink_client.h"
#include "pdu/pdu_store.h"
#include "power/power_history_store.h"
#include "power/power_sequence_engine.h"
#include "power/power_budget_controller.h"
#include "update_manager.h"
#include "ui/app_shell.h"
#include <lvgl.h>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <fstream>

#ifdef EMULATOR_BUILD
#  include <src/drivers/sdl/lv_sdl_window.h>
#  include <src/drivers/sdl/lv_sdl_mouse.h>
#  include <src/drivers/sdl/lv_sdl_mousewheel.h>
#  include <src/drivers/sdl/lv_sdl_keyboard.h>
#  include <src/others/snapshot/lv_snapshot.h>
#else
#  include <src/drivers/display/fb/lv_linux_fbdev.h>
#  include <src/drivers/evdev/lv_evdev.h>
#endif

static std::atomic<bool> g_running = true;
static void on_signal(int) { g_running = false; }

#ifdef EMULATOR_BUILD
static void inject_mock_data(MetricsStore& store, PduStore& pdu_store) {
    auto make_host = [&](const char* name, float cpu, float temp, float ram_pct,
                          uint64_t total_mb, int outlet) {
        protocol::HelloMessage h;
        h.host = name; h.arch = "aarch64"; h.os = "Linux 6.6.51"; h.outlet = outlet;
        h.services = {{"docker"}, {"nginx"}};
        h.scripts = {{"Update Containers", "docker compose pull && docker compose up -d"}};
        store.update_hello(h);

        protocol::Metrics m;
        m.host = name;
        m.ts   = std::time(nullptr);
        m.cpu.usage_pct = cpu;
        m.cpu.core_pct  = {cpu - 4, cpu + 2, cpu - 1, cpu + 3};
        m.cpu_temp_c    = temp;
        m.memory.total_kb = total_mb * 1024;
        m.memory.used_kb  = (uint64_t)(m.memory.total_kb * ram_pct / 100.f);
        m.memory.pct      = ram_pct;
        m.uptime.uptime_s = 86400 * 3 + 7200;
        m.uptime.load1    = cpu / 100.f * 4;
        m.uptime.load5    = cpu / 100.f * 3.5f;
        m.uptime.load15   = cpu / 100.f * 3.f;

        protocol::DiskMount root;
        root.path = "/"; root.total_kb = 32*1024*1024; root.used_kb = 8*1024*1024;
        root.pct = 25.f; root.read_kbs = 12.f; root.write_kbs = 4.f;
        m.disks.push_back(root);

        protocol::NetIface eth;
        eth.name = "eth0"; eth.rx_bps = 128*1024; eth.tx_bps = 32*1024;
        m.net.push_back(eth);

        store.update_metrics(m);
    };

    // make_host("control-pi",  18.f, 52.1f, 41.f, 1024, 0);
    // make_host("storage",     43.f, 59.7f, 55.f, 8192, 1);
    // make_host("compute-01",  94.f, 78.4f, 78.f, 16384, 2);
    // make_host("services",     6.f, 44.0f, 22.f, 4096, 3);

    protocol::PduSnapshot snapshot;
    for (int i = 1; i <= 8; ++i) {
        protocol::OutletState o;
        o.outlet = i;
        o.name   = "Outlet " + std::to_string(i);
        o.on     = (i != 5 && i != 8);
        snapshot.outlets.push_back(o);
    }
    snapshot.inlet_amps = 1.25f;
    snapshot.nominal_volts = 120.f;
    snapshot.estimated_watts = 150.f;
    snapshot.measurements_available = true;
    pdu_store.update(std::move(snapshot));
}

static bool save_screenshot(const char* path) {
    lv_draw_buf_t* snapshot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_XRGB8888);
    if (!snapshot) return false;
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << snapshot->header.w << " " << snapshot->header.h << "\n255\n";
    for (uint32_t y = 0; y < snapshot->header.h; ++y) {
        auto* row = reinterpret_cast<const uint32_t*>(
            static_cast<const uint8_t*>(snapshot->data) + y * snapshot->header.stride);
        for (uint32_t x = 0; x < snapshot->header.w; ++x) {
            uint32_t px = row[x];
            char rgb[3] = {
                static_cast<char>((px >> 16) & 0xff),
                static_cast<char>((px >> 8) & 0xff),
                static_cast<char>(px & 0xff)
            };
            out.write(rgb, sizeof(rgb));
        }
    }
    lv_draw_buf_destroy(snapshot);
    return out.good();
}
#endif

int main(int argc, char* argv[]) {
    // In emulator mode fall back to a local config.toml if the system path doesn't exist
#ifdef EMULATOR_BUILD
    std::string cfg_path = "config.toml";
#else
    std::string cfg_path = "/etc/command-deck/config.toml";
#endif
    if (argc > 1) cfg_path = argv[1];

    auto cfg = Config::load(cfg_path);

    // ── LVGL init ─────────────────────────────────────────────────────────────
    lv_init();

#ifdef EMULATOR_BUILD
    lv_display_t* disp = lv_sdl_window_create(cfg.display.width, cfg.display.height);
    lv_sdl_window_set_title(disp, "Command Deck — Emulator");
    lv_indev_t* mouse      = lv_sdl_mouse_create();
    lv_indev_t* mousewheel = lv_sdl_mousewheel_create();
    lv_indev_t* kb         = lv_sdl_keyboard_create();
    (void)mouse; (void)mousewheel; (void)kb;
#else
    lv_display_t* disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, cfg.display.fb_device.c_str());
    lv_indev_t* touch = lv_evdev_create(LV_INDEV_TYPE_POINTER,
                                         cfg.display.touch_device.c_str());
    (void)touch;
#endif

    // ── Data layer ────────────────────────────────────────────────────────────
#ifdef EMULATOR_BUILD
    MetricsStore store;
#else
    MetricsStore store(cfg.server.cache_file);
#endif
    PduStore     pdu_store;
    ActivityStore activity;
    PowerHistoryStore power_history(cfg.power);

    std::unique_ptr<SynlinkClient> pdu_client;
    if (cfg.pdu.enabled)
        pdu_client = std::make_unique<SynlinkClient>(cfg.pdu);

    WsServer* ws_server_ptr = nullptr;
    CommandRouter router(pdu_client.get(), activity,
        [&](const std::string& host, const std::string& msg) -> bool {
            return ws_server_ptr && ws_server_ptr->send_command(host, msg);
        });

    WsServer ws_server(store, activity, cfg.server.port);
    ws_server_ptr = &ws_server;
    PowerSequenceEngine sequences(cfg, store, pdu_store, router, activity);
    PowerBudgetController budgets(cfg, pdu_store, sequences);
    UpdateManager updater(cfg.update);

    // ── Background threads ────────────────────────────────────────────────────
    std::thread ws_thread([&]{ ws_server.run(); });

    std::thread pdu_thread([&]{
        while (g_running) {
            if (pdu_client) {
                auto snapshot = pdu_client->get_snapshot();
                if (snapshot && !snapshot->outlets.empty()) {
                    power_history.record(*snapshot);
                    pdu_store.update(std::move(*snapshot));
                    power_history.rollup_and_cleanup();
                } else {
                    pdu_store.mark_poll_failed();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.pdu.poll_ms));
        }
    });

    // ── UI ────────────────────────────────────────────────────────────────────
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    AppShell shell(store, pdu_store, activity, router, power_history, sequences, budgets,
                   updater, cfg);
    shell.build();

#ifdef EMULATOR_BUILD
    inject_mock_data(store, pdu_store);
    store.seed_mock_history();
    shell.refresh();
    if (const char* tab = std::getenv("DECK_SCREENSHOT_TAB")) {
        std::string name = tab;
        if (name == "hosts") shell.show_tab(AppShell::TAB_HOSTS);
        else if (name == "power") shell.show_tab(AppShell::TAB_PDU);
        else if (name == "system") shell.show_tab(AppShell::TAB_SETTINGS);
    }
    if (const char* host = std::getenv("DECK_SCREENSHOT_HOST"))
        shell.show_host_detail(host);
    if (const char* screenshot = std::getenv("DECK_SCREENSHOT")) {
        for (int i = 0; i < 3; ++i) lv_timer_handler();
        save_screenshot(screenshot);
        g_running = false;
    }
#endif

    while (g_running) {
        uint32_t delay = lv_timer_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }

    ws_server.stop();
    ws_thread.join();
    pdu_thread.join();
    return 0;
}
