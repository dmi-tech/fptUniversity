#!/usr/bin/env bash
# Install (if needed) and run a Mosquitto MQTT broker for the MQTT lab.
# Anonymous access, no TLS: for a trusted lab network only.
#
# Usage: scripts/mqtt_broker.sh        (Ctrl+C to stop)
set -euo pipefail
die() { echo "[ERR] $*" >&2; exit 1; }

if ! command -v mosquitto >/dev/null; then
	echo "mosquitto not found, installing (needs sudo)..."
	sudo apt-get install -y mosquitto mosquitto-clients
fi

conf=$(mktemp)
trap 'rm -f "$conf"' EXIT
cat >"$conf" <<'CONF'
listener 1883
allow_anonymous true
CONF

ip=$(hostname -I | awk '{print $1}')
echo "=== Broker address for the board: $ip  port 1883 ==="
echo "Virtual machine: set the network adapter to Bridged so the board can reach it."
if command -v ufw >/dev/null && sudo -n ufw status 2>/dev/null | grep -q "Status: active"; then
	echo "Firewall is active: sudo ufw allow 1883"
fi
echo "Test: mosquitto_sub -h $ip -t '#' -v"
mosquitto -c "$conf" -v
