#!/usr/bin/env bash
# Build fw_showcase (MCUboot + signed application) with sysbuild. Never flashes.
#
# Signing key: STM32H573_MCUBOOT_KEY_FILE (RSA-2048 private key, required;
# no key is stored in this repository).
#
# Usage: scripts/build.sh [build_dir]   (default: <fw_showcase>/build)
# Run scripts/setup.sh once first.
set -euo pipefail
app_dir=$(cd "$(dirname "$0")/.." && pwd)
build_dir=$(realpath -m "${1:-$app_dir/build}")
key=${STM32H573_MCUBOOT_KEY_FILE:-}

# Use the workspace virtualenv created by setup.sh, if there is one
ws=$(cd "$(git -C "$app_dir" rev-parse --show-toplevel)/.." && pwd)
if [ -x "$ws/.venv/bin/west" ]; then
	export PATH="$ws/.venv/bin:$PATH"
fi
command -v west >/dev/null || {
	echo "[ERR] west not found. Run $app_dir/scripts/setup.sh first." >&2
	exit 2
}

if [ -z "$key" ] || [ ! -r "$key" ]; then
	echo "[ERR] Signing key not found: ${key:-<unset>}" >&2
	echo "      Set STM32H573_MCUBOOT_KEY_FILE to the RSA-2048 private key." >&2
	exit 2
fi
key=$(realpath "$key")

cd "$ws"
west build -p always -d "$build_dir" -b stm32h573ri_custom --sysbuild "$app_dir" \
	-- "-DBOARD_ROOT=$app_dir" \
	"-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$key\""
echo "=== DONE fw_showcase build: $build_dir ==="
