#!/usr/bin/env bash
# Build fw_showcase (see build.sh), then flash MCUboot + the signed
# application over ST-LINK SWD (STM32_Programmer_CLI must be on PATH).
#
# Usage: scripts/flash.sh [build_dir]   (default: <fw_showcase>/build)
set -euo pipefail
app_dir=$(cd "$(dirname "$0")/.." && pwd)
build_dir=$(realpath -m "${1:-$app_dir/build}")

command -v STM32_Programmer_CLI >/dev/null || {
	echo "[ERR] STM32_Programmer_CLI not found. Install STM32CubeProgrammer" >&2
	echo "      (https://www.st.com/en/development-tools/stm32cubeprog.html)" >&2
	echo "      and add its bin/ directory to PATH." >&2
	exit 2
}

"$app_dir/scripts/build.sh" "$build_dir"

ws=$(cd "$(git -C "$app_dir" rev-parse --show-toplevel)/.." && pwd)
if [ -x "$ws/.venv/bin/west" ]; then
	export PATH="$ws/.venv/bin:$PATH"
fi
cd "$ws"
west flash -d "$build_dir" --skip-rebuild
echo "=== DONE fw_showcase flash ==="
