#!/usr/bin/env bash
# Build EmbeddedCourse_FPT_DMI (MCUboot + signed application) with sysbuild.
# Never flashes.
#
# Signing key: STM32H573_MCUBOOT_KEY_FILE (RSA-2048 private key). When unset,
# the PUBLIC MCUboot development key is used. It is for lab use only: anyone
# can sign an image that this bootloader accepts.
#
# Usage: scripts/build.sh [build_dir] [extra west/cmake args after --]
#        (default build_dir: <project>/build)
# Run scripts/setup.sh once first.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

build_dir=$(realpath -m "${1:-$app_dir/build}")
[ $# -gt 0 ] && shift
[ "${1:-}" = "--" ] && shift
use_venv

key=${STM32H573_MCUBOOT_KEY_FILE:-}
if [ -n "$key" ]; then
	[ -r "$key" ] || die "STM32H573_MCUBOOT_KEY_FILE is not readable: $key"
else
	key="$ws/bootloader/mcuboot/root-rsa-2048.pem"
	[ -r "$key" ] || die "development key not found: $key (run scripts/setup.sh)"
	echo "[WARN] STM32H573_MCUBOOT_KEY_FILE is not set: signing with the public"
	echo "       MCUboot development key. Lab use only."
fi
key=$(realpath "$key")

cd "$ws"
west build -p always -d "$build_dir" -b stm32h573ri_custom --sysbuild "$app_dir" \
	-- "-DBOARD_ROOT=$app_dir" \
	"-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$key\"" "$@"
echo "=== DONE build: $build_dir ==="
