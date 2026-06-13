#!/usr/bin/env bash
# Run once on a fresh Raspberry Pi OS Lite (64-bit) install.
# Strips unnecessary services/packages and configures the Pi for headless kiosk operation.
set -euo pipefail

if [[ $EUID -ne 0 ]]; then echo "Run as root."; exit 1; fi

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

echo "==> Removing unnecessary packages"
apt-get remove -y --purge \
    bluez bluetooth pi-bluetooth \
    triggerhappy \
    avahi-daemon \
    cups cups-browsed \
    nfs-common \
    rpcbind \
    samba samba-common-bin \
    wolfram-engine scratch \
    libreoffice* \
    2>/dev/null || true

apt-get autoremove -y --purge
apt-get clean

echo "==> Installing runtime dependencies"
apt-get update
apt-get install -y \
    libwebsockets-dev \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    libgpiod-dev \
    cmake \
    build-essential \
    git \
    evtest       # useful for identifying touch device paths

echo "==> Disabling unused services"
SERVICES=(
    display-manager.service
    lightdm.service
    gdm.service
    gdm3.service
    sddm.service
    bluetooth.service
    hciuart.service
    triggerhappy.service
    avahi-daemon.service
    ModemManager.service
    cups.service
    cups-browsed.service
)
for svc in "${SERVICES[@]}"; do
    systemctl disable "$svc" 2>/dev/null || true
    systemctl stop    "$svc" 2>/dev/null || true
done

echo "==> Switching boot target to text console"
systemctl set-default multi-user.target

echo "==> Disabling swap (optional — comment out if you need it)"
systemctl disable dphys-swapfile.service 2>/dev/null || true
dphys-swapfile swapoff 2>/dev/null || true
dphys-swapfile uninstall 2>/dev/null || true

echo "==> Configuring /boot/firmware/config.txt"
CFG=/boot/firmware/config.txt
# Minimal GPU memory — no X11, LVGL owns /dev/fb0 directly
grep -q 'gpu_mem=' "$CFG" && sed -i 's/gpu_mem=.*/gpu_mem=16/' "$CFG" \
    || echo 'gpu_mem=16' >> "$CFG"

# Disable Bluetooth
grep -q 'dtoverlay=disable-bt' "$CFG" || echo 'dtoverlay=disable-bt' >> "$CFG"

# Disable WiFi if you're on Ethernet only (comment out if you need WiFi)
# grep -q 'dtoverlay=disable-wifi' "$CFG" || echo 'dtoverlay=disable-wifi' >> "$CFG"

# Official 7" DSI touchscreen
grep -q 'dtoverlay=vc4-kms-dsi-7inch' "$CFG" || echo 'dtoverlay=vc4-kms-dsi-7inch' >> "$CFG"

echo "==> Hiding the console cursor"
CMDLINE=/boot/firmware/cmdline.txt
grep -q 'vt.global_cursor_default=0' "$CMDLINE" || \
    sed -i 's/$/ vt.global_cursor_default=0/' "$CMDLINE"

echo "==> Disabling tty1 login prompt"
systemctl disable getty@tty1.service 2>/dev/null || true
systemctl mask getty@tty1.service 2>/dev/null || true
rm -f /etc/systemd/system/getty@tty1.service.d/autologin.conf
sed -i 's/\bconsole=tty1\b//g; s/  */ /g; s/ $//' "$CMDLINE"

echo "==> Adjusting journald to limit log size"
sed -i 's/#SystemMaxUse=/SystemMaxUse=50M/' /etc/systemd/journald.conf
systemctl restart systemd-journald

echo "==> Installing dashboard updater"
bash "$SCRIPT_DIR/install-updater.sh"

echo ""
echo "==> Done. Build and install command-deck, then:"
echo "    systemctl enable --now command-deck"
echo "    Reboot to apply config.txt changes."
