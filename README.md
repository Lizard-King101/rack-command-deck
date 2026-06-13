# Command Deck

Homelab rack control panel — LVGL UI on a Raspberry Pi 3B+ with a 7" touchscreen,
plus a lightweight metrics agent that runs on every machine in the rack.

```
control-pi/      — C++ LVGL dashboard (runs on the display Pi)
metrics-agent/   — C++ metrics + control agent (runs on every managed machine)
shared/          — protocol.h shared between both
```

---

## What it does

The control Pi opens a persistent WebSocket server. Each managed machine runs
`deck-agent`, which connects on boot, sends a `hello` (hostname, arch, OS, PDU
outlet, services, scripts), then streams a `metrics` frame every 2 s. The UI
refreshes from an in-memory store with no polling delay.

**UI screens (800×480 touchscreen):**

| Tab | Description |
|---|---|
| Overview | Arc-gauge stats panel (online count, avg CPU%, avg RAM%, estimated rack watts), scrolling alert ticker for offline/hot hosts, tappable host rows |
| Hosts | Full host-card grid — compact cards with CPU/RAM bars, temp, PDU state, status strip |
| Power | Rack budget/energy summary, outlets, 30-day history, and power sequences |
| System | Server/display config, connected-agent count, and recent command activity |
| Host Detail | Full-screen per-host view (stats + 24h charts + power controls + services/scripts + machine config) |

**Host Detail features:**
- Power row: Reboot, Shutdown, Graceful Off (waits for host offline then cuts outlet), Wake-on-LAN, Outlet Toggle
- 24h history line charts for CPU%, temp, RAM%
- Service control (start/stop/restart) — services defined in the agent's `agent.toml`
- One-shot script buttons — scripts defined in the agent's `agent.toml`
- Machine config CRUD: rename display name, override PDU outlet, set MAC, remove from cache
- Recent command activity with pending/success/failure output

Commands receive unique lifecycle IDs and are tracked from dispatch through
completion. Results appear as a global toast, in the System tab, and in the
target host's detail screen. Local PDU and Wake-on-LAN actions use the same
activity flow as agent commands.

---

## Prerequisites

### Dev machine (CachyOS / Arch)

```bash
sudo pacman -S cmake libwebsockets sqlite
# SDL2 and curl are on a standard CachyOS install already
```

The first CMake run downloads these via FetchContent (needs git + internet):
- **LVGL 9.2.0** — UI library
- **nlohmann/json 3.11.3** — JSON serialisation
- **toml++ 3.4.0** — config parsing

### Managed machines (agent)

```bash
# Debian / Raspberry Pi OS
sudo apt install libwebsockets-dev cmake build-essential git

# Arch / Manjaro
sudo pacman -S libwebsockets cmake
```

---

## Building

### control-pi — emulator (dev machine)

```bash
cd control-pi
cmake -B build -DEMULATOR=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/command-deck config.toml
```

Opens an 800×480 SDL2 window. Mouse = touch.

### control-pi — Pi target (manual build fallback)

The normal Pi update path downloads the ARM64 release built by GitHub Actions.
To build natively on the Pi instead:

```bash
sudo apt install libwebsockets-dev libcurl4-openssl-dev libsqlite3-dev cmake build-essential git
cd control-pi
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
sudo systemctl enable --now command-deck
```

### Updating from the touchscreen

Every successful push to `master` builds and tests the dashboard in a native
ARM64 Debian Bookworm environment, then replaces the assets on the rolling
`latest` GitHub prerelease. The System tab downloads, verifies, installs, and
restarts that release while showing progress.

Configure `/etc/command-deck/config.toml`:

```toml
[update]
enabled = true
release_url = "https://github.com/Lizard-King101/rack-command-deck/releases/download/latest/command-deck-linux-arm64.tar.gz"
helper_path = "/usr/local/libexec/command-deck-update"
```

Install the root-owned helper and its narrowly scoped sudo rule once:

```bash
sudo bash control-pi/scripts/install-updater.sh
```

The updater requires `curl`, `file`, `tar`, and `sha256sum`. It verifies the
release checksum, archive contents, and ARM64 architecture before atomically
installing the dashboard binary. The previous binary is retained at
`/usr/local/bin/command-deck.previous`.

Changes to the root-owned updater helper require rerunning `install-updater.sh`
manually. To migrate an existing installation, perform one final touchscreen
update with the old source-build updater so the new dashboard binary is
installed. Then rerun `install-updater.sh` and replace `repo_path` with
`release_url` in `/etc/command-deck/config.toml`.

### GitHub Actions release cache

The release workflow caches `control-pi/build-release` and `ccache` output.
CMake configuration still runs on every build, but unchanged FetchContent
dependencies and compiled objects are reused.

For changes that require a completely clean state, increment `CACHE_EPOCH` in
`.github/workflows/release-master.yml` before pushing. To remove all existing
repository caches manually, use:

```bash
gh cache delete --all --confirm
```

Caches can also be removed under the repository's **Actions > Caches** page.
Changing the cache epoch is the preferred documented reset because it guarantees
the next release build cannot restore an older cache.

### metrics-agent

```bash
cd metrics-agent
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
sudo systemctl enable --now deck-agent
```

To enable GPIO control on a Pi:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_GPIO=ON
```

---

## Iterating on the UI (emulator workflow)

```bash
# From control-pi/:
cmake --build build -j$(nproc) && ./build/command-deck config.toml
```

Only re-run `cmake -B build` when adding new `.cpp` files or changing `CMakeLists.txt`.

For headless visual checks, the emulator can write a screenshot:

```bash
SDL_VIDEODRIVER=dummy DECK_SCREENSHOT=/tmp/overview.ppm ./build/command-deck config.toml
SDL_VIDEODRIVER=dummy DECK_SCREENSHOT_TAB=system DECK_SCREENSHOT=/tmp/system.ppm ./build/command-deck config.toml
SDL_VIDEODRIVER=dummy DECK_SCREENSHOT_HOST=compute-01 DECK_SCREENSHOT=/tmp/host.ppm ./build/command-deck config.toml
```

Files you'll touch most for visual work:

| File | What it controls |
|---|---|
| `src/ui/styles.cpp` | Colour palette, fonts, LVGL style objects |
| `src/ui/screen_overview.cpp` | Overview tab — arc gauges, alert ticker, host rows |
| `src/ui/screen_hosts.cpp` | Hosts tab — host card grid |
| `src/ui/host_card_wide.cpp` | Per-host compact card widget |
| `src/ui/screen_host_detail.cpp` | Full-screen host panel — stats, charts, controls |
| `src/ui/app_shell.cpp` | Header bar, tab bar, screen switching, detail-mode header swap |

---

## Configuration

### control-pi — `control-pi/config.toml`

```toml
[server]
port = 8765          # WebSocket port agents connect to
bind = "0.0.0.0"

[display]
width  = 800
height = 480
fb_device    = "/dev/fb0"
touch_device = "/dev/input/event0"   # confirm with: evtest

[pdu]
enabled  = true
host     = "192.168.1.5"   # Synlink PDU IP
port     = 80
access_token = "replace-with-personal-access-token"
nominal_voltage = 120  # used with inlet RMS current to estimate rack watts
poll_ms  = 5000

[power]
database_path = "power.db"
currency = "USD"
cost_per_kwh = 0.15
warning_watts = 600
critical_watts = 750
critical_hold_s = 30
load_shedding_enabled = false
startup_readiness_s = 10

[[power_group]]
name = "compute"
members = ["compute-01"]
dependencies = ["core"]
shedding_priority = 100
never_shed = false
startup_timeout_s = 120
shutdown_timeout_s = 120
```

### metrics-agent — `metrics-agent/agent.toml`

```toml
[connection]
control_host = "192.168.1.10"
control_port = 8765
reconnect_ms = 5000

[metrics]
interval_ms = 2000
cpu = true
memory = true
disk = true
network = true
temperature = true
uptime = true

[controls]
allow_reboot     = true
allow_shutdown   = false
allow_gpio       = false
allowed_services = ["nginx", "docker", "sshd"]

[pdu]
outlet = 3   # which PDU outlet this machine is on (0 = unassigned)

# Services exposed for start/stop/restart from the deck UI
[[services]]
name = "nginx"

# One-shot scripts the deck can trigger
[[scripts]]
name    = "Update Packages"
command = "apt-get update -y && apt-get upgrade -y"
```

Services and scripts are defined here per machine — the control Pi picks them up
from the `hello` message on connect. No changes needed on the control Pi side.

---

## OS setup (control Pi only)

Run once on a fresh **Raspberry Pi OS Lite 64-bit** install:

```bash
sudo bash control-pi/scripts/setup-os.sh
sudo reboot
```

Strips Bluetooth, avahi, triggerhappy; sets `gpu_mem=16`; enables the 7" DSI
touchscreen overlay. LVGL writes directly to `/dev/fb0` — no X11 needed.

---

## Project structure

```
command-deck/
├── shared/
│   └── protocol.h               — JSON message types (both projects include this)
│
├── control-pi/
│   ├── CMakeLists.txt
│   ├── lv_conf.h                 — LVGL feature flags
│   ├── config.toml               — runtime config (not compiled in)
│   └── src/
│       ├── main.cpp              — entry point, wires all subsystems together
│       ├── config.{h,cpp}        — TOML config loading
│       ├── metrics_store.{h,cpp} — thread-safe store of host data + 24h ring buffer
│       ├── ws_server.{h,cpp}     — libwebsockets server (agents connect here)
│       ├── command_router.{h,cpp}— routes UI commands to agents or PDU
│       ├── wol.{h,cpp}           — sends Wake-on-LAN magic packets
│       ├── pdu/
│       │   ├── synlink_client.{h,cpp}  — HTTP REST client for Synlink PDU
│       │   └── pdu_store.{h,cpp}       — latest outlet state cache
│       └── ui/
│           ├── styles.{h,cpp}           — colour palette + LVGL style objects
│           ├── app_shell.{h,cpp}        — root layout: header, tabs, screen switching
│           ├── screen_overview.{h,cpp}  — Overview tab (arc gauges, ticker, rows)
│           ├── screen_hosts.{h,cpp}     — Hosts tab (card grid)
│           ├── host_card_wide.{h,cpp}   — per-host compact card widget
│           ├── screen_pdu.{h,cpp}       — PDU tab (outlet table)
│           ├── screen_settings.{h,cpp}  — System/config/activity tab
│           └── screen_host_detail.{h,cpp} — full-screen host panel + CRUD
│
└── metrics-agent/
    ├── CMakeLists.txt
    ├── agent.toml
    └── src/
        ├── main.cpp
        ├── config.{h,cpp}            — TOML config (includes services + scripts)
        ├── ws_client.{h,cpp}         — reconnecting WebSocket client
        ├── collector/
        │   ├── collector.h
        │   ├── cpu.cpp               — /proc/stat
        │   ├── memory.cpp            — /proc/meminfo
        │   ├── disk.cpp              — /proc/diskstats + statvfs
        │   ├── network.cpp           — /proc/net/dev
        │   ├── temperature.cpp       — /sys/class/thermal
        │   └── uptime.cpp            — /proc/uptime + /proc/loadavg
        └── control/
            ├── handler.{h,cpp}       — command dispatch
            ├── host_ctrl.{h,cpp}     — reboot / shutdown
            ├── service_ctrl.{h,cpp}  — systemctl wrapper
            └── gpio_ctrl.{h,cpp}     — libgpiod (needs -DENABLE_GPIO=ON)
```

---

## WebSocket message protocol

All messages are JSON with a `type` field. See `shared/protocol.h` for the full structs.

| Direction | type | When |
|---|---|---|
| agent → control | `hello` | on connect — hostname, arch, OS, PDU outlet, services, scripts |
| agent → control | `metrics` | every `interval_ms` — CPU, RAM, disk, net, temp, uptime |
| control → agent | `cmd` | user triggers an action from the UI |
| agent → control | `cmd_result` | response to a cmd |

### Supported `cmd` actions

| action | params | handled by |
|---|---|---|
| `reboot` | — | agent (`allow_reboot = true`) |
| `shutdown` | — | agent (`allow_shutdown = true`) |
| `service` | `service_name`, `service_op` (start/stop/restart) | agent |
| `script` | `service_name` = shell command | agent |
| `gpio` | `gpio_pin`, `gpio_state` | agent (`allow_gpio = true` + `ENABLE_GPIO`) |
| `outlet` | `outlet_num`, `outlet_state` | control Pi → PDU directly |
| `wol` | `service_name` = MAC address | control Pi sends magic packet |

### Graceful shutdown flow

"Graceful Off" sends `shutdown` to the agent, waits for it to go offline, then
cuts its assigned PDU outlet. Timeout: 2 min waiting for offline.

---

## Synlink PDU notes

`synlink_client.cpp` uses a SynOS personal access token, polls `/api/outlets`
for switching state and `/api/inlets` for total inlet current, and sends PUT
requests to toggle outlets. For current-only PDU models, estimated rack watts
are calculated as inlet RMS current times the configured nominal voltage.

```bash
curl -H 'Authorization: Bearer <access-token>' \
  http://<pdu-ip>/api/inlets | python3 -m json.tool
```
