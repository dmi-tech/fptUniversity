#!/usr/bin/env bash
# Build every lab example of docs/examples/ (app only, nothing is flashed).
# Usage: tests/lab_examples/run.sh [lab ...]     (default: all)
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
app_dir=$(cd "$here/../.." && pwd)
source "$app_dir/scripts/common.sh"
use_venv
build_root=${BUILD_ROOT:-$here}

# name | example file | drivers | extra Kconfig
declare -A FILE=( [lab1]=lab1_shell.c [lab2a]=lab2a_button_relay.c [lab2b]=lab2b_sensor_buzzer.c
	[lab3a]=lab3a_can_relay.c [lab7]=lab7_mcuboot.c )
declare -A DRV=( [lab1]="peripherals/gpio" [lab2a]="peripherals/gpio"
	[lab2b]="peripherals/gpio;peripherals/adc" [lab3a]="peripherals/gpio;protocols/can" [lab7]="" )
declare -A CONF=(
	[lab1]="CONFIG_SHELL=y CONFIG_SHELL_BACKEND_RTT=y CONFIG_LOG=y CONFIG_LOG_PRINTK=y"
	[lab2a]="" [lab2b]="CONFIG_ADC=y CONFIG_SENSOR=y CONFIG_CBPRINTF_FP_SUPPORT=y"
	[lab3a]="CONFIG_CAN=y"
	[lab7]="CONFIG_BOOTLOADER_MCUBOOT=y CONFIG_FLASH=y CONFIG_FLASH_MAP=y CONFIG_STREAM_FLASH=y CONFIG_IMG_MANAGER=y" )

if [ $# -gt 0 ]; then names=("$@"); else names=(lab1 lab2a lab2b lab3a lab7); fi
fail=0
for n in "${names[@]}"; do
	step "lab_examples: $n"
	args=()
	for kv in ${CONF[$n]}; do args+=("-D$kv"); done
	extra_overlay=""
	[ "$n" = lab2b ] && extra_overlay="$here/adc.overlay"
	if (cd "$ws" && west build -p always -d "$build_root/build-$n" -b stm32h573ri_custom "$here" -- \
		"-DBOARD_ROOT=$app_dir" "-DLAB_MAIN=${FILE[$n]}" "-DLAB_DRIVERS=${DRV[$n]}" \
		"-DEXTRA_DTC_OVERLAY_FILE=$here/lab.overlay${extra_overlay:+;$extra_overlay}" \
		"${args[@]}") >"$build_root/build-$n.log" 2>&1; then
		echo "PASS $n"
	else
		echo "FAIL $n (log: $build_root/build-$n.log)"
		fail=1
	fi
done
exit $fail
