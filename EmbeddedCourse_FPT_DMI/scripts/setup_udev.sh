#!/usr/bin/env bash
# Linux: let a normal user open ST-LINK / J-Link USB probes and serial ports.
# Needs sudo. Run once, then unplug and replug the probe.
#
# Usage: scripts/setup_udev.sh
set -euo pipefail

rules=/etc/udev/rules.d/60-embeddedcourse-probes.rules
user=${SUDO_USER:-$USER}

echo "=== Writing $rules ==="
# ST-LINK (VID 0483): V2, V2-1, V3 and variants. J-Link (VID 1366).
sudo tee "$rules" >/dev/null <<'RULES'
# ST-LINK
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3744|3748|374b|3752|374d|374e|374f|3753|3754|3757", MODE="0660", GROUP="plugdev", TAG+="uaccess"
# SEGGER J-Link
SUBSYSTEMS=="usb", ATTRS{idVendor}=="1366", MODE="0660", GROUP="plugdev", TAG+="uaccess"
RULES

echo "=== Reloading udev rules ==="
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "=== Adding $user to plugdev and dialout ==="
sudo usermod -aG plugdev,dialout "$user"

cat <<MSG

Done. Now:
  1. Unplug and replug the probe.
  2. Log out and log in again (the new groups apply to new sessions only).

Virtual machine: attach the probe to the VM (VirtualBox: Devices > USB;
VMware: VM > Removable Devices). Attach it to the VM only, not to the host.
MSG
