#!/usr/bin/env bash
# Build one driver README example as a signed MCUboot image, flash it and capture the RTT log.
#
# Flashes the board, so it refuses to run unless HW_FLASH_OK=1 is set.
#
# Usage: HW_FLASH_OK=1 tests/hw/run_case.sh <driver> [seconds]
#        <driver>: directory name of the driver, e.g. gpio, timer, sht41 (default 10 s of log)
# Output: tests/hw/logs/<driver>-<date>.log ; checks tests/hw/expect/<driver>.re if it exists
#         (one extended regex per line, every line must match somewhere in the log).
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
app_dir=$(cd "$here/../.." && pwd)
source "$app_dir/scripts/common.sh"
export PATH="$HOME/.local/bin:$PATH"

drv=${1:?usage: $0 <driver> [seconds]}
secs=${2:-10}
[ "${HW_FLASH_OK:-}" = 1 ] || die "this flashes the board: set HW_FLASH_OK=1 to confirm"
use_venv

# 1. The app: tests/hw/apps/<case> if it exists, else the example of the driver README
#    (same parser as tests/readme_examples)
if [ -d "$here/apps/$drv" ]; then
	gen=$here/gen/custom_$drv
	rm -rf "$gen" && mkdir -p "$here/gen" && cp -r "$here/apps/$drv" "$gen"
else
	readme=$(ls "$app_dir"/driver/*/"$drv"/README.md 2>/dev/null | head -1)
	[ -n "$readme" ] || die "no driver or case '$drv'"
	group=$(basename "$(dirname "$(dirname "$readme")")")
	gen_root=$here/gen
	python3 -I "$app_dir/tests/readme_examples/build_examples.py" "$app_dir" "$ws" "$gen_root" "$drv" >/dev/null || true
	gen=$gen_root/apps/${group}_${drv}
	[ -d "$gen" ] || die "example not generated for $drv"
fi
cp "$app_dir/sysbuild.conf" "$gen/sysbuild.conf"
mkdir -p "$gen/sysbuild" && cp "$app_dir/sysbuild/mcuboot.conf" "$gen/sysbuild/mcuboot.conf"
grep -q BOOTLOADER_MCUBOOT "$gen/prj.conf" || echo "CONFIG_BOOTLOADER_MCUBOOT=y" >> "$gen/prj.conf"
grep -q STM32_ENABLE_DEBUG_SLEEP_STOP "$gen/prj.conf" || echo "CONFIG_STM32_ENABLE_DEBUG_SLEEP_STOP=y" >> "$gen/prj.conf"

# 2. Build (signing key as in scripts/build.sh)
key=${STM32H573_MCUBOOT_KEY_FILE:-$ws/bootloader/mcuboot/root-rsa-2048.pem}
bdir=$here/build-$drv
step "hw: build $drv"
(cd "$ws" && west build -p always -d "$bdir" -b stm32h573ri_custom --sysbuild "$gen" -- \
	"-DBOARD_ROOT=$app_dir" "-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\"$key\"") >"$bdir.log" 2>&1 \
	|| { tail -20 "$bdir.log"; die "build failed ($bdir.log)"; }

# 3. Flash
step "hw: flash $drv"
# The SWD connection sometimes fails on the first try ("Unable to get core ID"); retry the whole
# flash (a failed connect does not touch the flash, a failed write is simply repeated).
flashed=0
for attempt in 1 2 3 4; do
	if (cd "$ws" && west flash -d "$bdir" --skip-rebuild --runner stm32cubeprogrammer) \
		>"$bdir-flash.log" 2>&1; then
		flashed=1
		break
	fi
	echo "flash attempt $attempt failed, retrying"
	sleep 2
done
[ $flashed = 1 ] || { tail -20 "$bdir-flash.log"; die "flash failed ($bdir-flash.log)"; }

# 4. Capture the RTT log from a fresh boot
step "hw: RTT $drv (${secs}s)"
log=$here/logs/$drv-$(date +%Y%m%d-%H%M%S).log
python "$app_dir/scripts/rtt_pyocd.py" --reset --seconds "$secs" 2>&1 | grep -v Overlapping | tee "$log"

# 5. Compare with the expected patterns
exp=$here/expect/$drv.re
if [ -f "$exp" ]; then
	rc=0
	while IFS= read -r re; do
		[ -z "$re" ] && continue
		grep -E -q -- "$re" "$log" || { echo "MISSING: $re"; rc=1; }
	done <"$exp"
	[ $rc = 0 ] && echo "RESULT $drv: PASS (log matches $exp)" || echo "RESULT $drv: FAIL"
	exit $rc
fi
echo "RESULT $drv: MANUAL (no expect file; judge the log: $log)"
