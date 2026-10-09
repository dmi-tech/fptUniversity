#!/usr/bin/env bash
# Build (see build.sh), then flash MCUboot + the signed application over SWD.
#
# Default runner: ST-LINK through STM32_Programmer_CLI (must be on PATH).
# Set RUNNER=jlink to use a J-Link instead.
#
# Usage: scripts/flash.sh [build_dir]   (default: <project>/build)
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

build_dir=$(realpath -m "${1:-$app_dir/build}")
runner=${RUNNER:-stm32cubeprogrammer}

case "$runner" in
stm32cubeprogrammer)
	command -v STM32_Programmer_CLI >/dev/null || {
		echo "[ERR] STM32_Programmer_CLI not found. Install STM32CubeProgrammer" >&2
		echo "      (https://www.st.com/en/development-tools/stm32cubeprog.html)" >&2
		echo "      and add its bin/ directory to PATH." >&2
		exit 2
	} ;;
jlink)
	command -v JLinkExe >/dev/null || die "JLinkExe not found (SEGGER J-Link software)" ;;
*) die "unknown RUNNER=$runner (use stm32cubeprogrammer or jlink)" ;;
esac

"$app_dir/scripts/build.sh" "$build_dir"

use_venv
cd "$ws"
west flash -d "$build_dir" --skip-rebuild --runner "$runner"
echo "=== DONE flash ==="
echo "Read the log with: $app_dir/scripts/rtt.sh"
