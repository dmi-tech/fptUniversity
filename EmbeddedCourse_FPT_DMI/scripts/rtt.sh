#!/usr/bin/env bash
# Show the application log (SEGGER RTT) without resetting or halting the board.
#
# - J-Link connected  -> JLinkRTTLogger (SEGGER J-Link software)
# - otherwise ST-LINK -> OpenOCD (scripts/rtt_stlink.cfg) + nc on port 19021
# - ST-LINK and OpenOCD without target/stm32h5x.cfg (or no OpenOCD) -> pyocd (scripts/rtt_pyocd.py)
#
# Usage: scripts/rtt.sh [options]        (Ctrl+C to stop)
#   Options are for the pyocd reader only: --reset (capture the boot log), --seconds N,
#   --all (also the old buffer content), --send TEXT (shell input). See rtt_pyocd.py.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
die() { echo "[ERR] $*" >&2; exit 1; }

if command -v JLinkRTTLogger >/dev/null && lsusb -d 1366: >/dev/null 2>&1; then
	echo "=== J-Link: JLinkRTTLogger (Ctrl+C to stop) ==="
	out=$(mktemp)
	trap 'rm -f "$out"' EXIT
	JLinkRTTLogger -Device STM32H573RI -If SWD -Speed 4000 -RTTChannel 0 "$out" &
	pid=$!
	trap 'kill $pid 2>/dev/null || true; rm -f "$out"' EXIT
	tail -f "$out"
	exit 0
fi

have_openocd_h5() {
	command -v openocd >/dev/null && \
		openocd -c "source [find target/stm32h5x.cfg]; exit" >/dev/null 2>&1
}
if ! have_openocd_h5; then
	source "$here/common.sh"
	use_venv
	python -c "import pyocd" 2>/dev/null || die "no OpenOCD with stm32h5x.cfg and no pyocd (pip install pyocd)"
	echo "=== ST-LINK: pyocd (Ctrl+C to stop) ===" >&2
	exec python "$here/rtt_pyocd.py" "$@"
fi
command -v nc >/dev/null || die "nc not found (Ubuntu/Debian: sudo apt install netcat-openbsd)"

echo "=== ST-LINK: OpenOCD + RTT on port 19021 (Ctrl+C to stop) ==="
openocd -f "$here/rtt_stlink.cfg" >/tmp/openocd-rtt.log 2>&1 &
pid=$!
trap 'kill $pid 2>/dev/null || true' EXIT
for _ in $(seq 1 50); do
	nc -z localhost 19021 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || { cat /tmp/openocd-rtt.log >&2; die "openocd stopped (log above)"; }
	sleep 0.2
done
nc localhost 19021
