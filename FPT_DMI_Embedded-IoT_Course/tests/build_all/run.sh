#!/usr/bin/env bash
# Build every configuration in configs/ (<name>.conf + <name>.overlay). Never flashes.
#
# Usage: tests/build_all/run.sh [config ...]     (default: all)
# Output: build directories under $BUILD_ROOT (default: tests/build_all/build-<name>)
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
app_dir=$(cd "$here/../.." && pwd)
source "$app_dir/scripts/common.sh"
use_venv

build_root=${BUILD_ROOT:-$here}
if [ $# -gt 0 ]; then
	names=("$@")
else
	names=()
	for f in "$here"/configs/*.overlay; do
		names+=("$(basename "$f" .overlay)")
	done
fi

fail=0
for name in "${names[@]}"; do
	step "build_all: $name"
	if (cd "$ws" && west build -p always -d "$build_root/build-$name" \
		-b stm32h573ri_custom "$here" -- \
		"-DBOARD_ROOT=$app_dir" \
		"-DEXTRA_CONF_FILE=$here/configs/$name.conf" \
		"-DEXTRA_DTC_OVERLAY_FILE=$here/configs/$name.overlay" \
		${EXTRA_CMAKE_ARGS:-}) >"$build_root/build-$name.log" 2>&1; then
		echo "PASS $name"
	else
		echo "FAIL $name (log: $build_root/build-$name.log)"
		fail=1
	fi
done
exit $fail
