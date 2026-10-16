#!/usr/bin/env bash
# Build this showcase (MCUboot + signed application) with sysbuild. Never flashes.
#
# Signing key: STM32H573_MCUBOOT_KEY_FILE (RSA-2048 private key, required;
# no key is stored in this repository).
#
# Usage: scripts/build.sh [--threads] [build_dir]   (default: <showcase>/build)
#   --threads  adds threads.conf: stack use / CPU of every thread on the
#              console every 30 s (to size the stacks; not for release)
# Run scripts/setup.sh once first.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

extra=()
if [ "${1:-}" = "--threads" ]; then
	extra=("-DEXTRA_CONF_FILE=$app_dir/threads.conf")
	shift
fi
build_dir=$(realpath -m "${1:-$app_dir/build}")
key=${STM32H573_MCUBOOT_KEY_FILE:-}
use_venv

if [ -z "$key" ] || [ ! -r "$key" ]; then
	echo "[ERR] Signing key not found: ${key:-<unset>}" >&2
	echo "      Set STM32H573_MCUBOOT_KEY_FILE to the RSA-2048 private key." >&2
	exit 2
fi
key=$(realpath "$key")

# sysbuild.conf and sysbuild/mcuboot.conf of this folder configure MCUboot;
# the board definition is in boards/st/
cd "$ws"
west build -p always -d "$build_dir" -b stm32h573ri_custom --sysbuild "$app_dir" \
	-- "-DBOARD_ROOT=$app_dir" \
	"-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$key\"" "${extra[@]}"
echo "=== DONE $app_name build: $build_dir ==="
