#!/usr/bin/env bash
# Lab 7: make an upgrade image for slot1 from a finished build (scripts/build.sh).
#
# Re-signs <build_dir>/FPT_DMI_Embedded-IoT_Course/zephyr/zephyr.bin with the given version, pads it to
# the slot size and adds the MCUboot trailer magic. Flashed at the start of slot1 it makes MCUboot
# swap it in on the next reset as a TEST image: not confirmed yet, so it is reverted unless the
# application calls boot_write_img_confirmed().
# Header size, alignment and slot size are the ones of the normal build (see build.ninja).
# Never flashes. Flash address of slot1: 0x080F0000 (flash base 0x08000000 + 0xF0000).
#
# Usage: scripts/lab7_upgrade_image.sh <build_dir> <version> [out.bin]
#        version: for example 2.0.0 (MCUboot version format major.minor.patch[+build])
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

[ $# -ge 2 ] || die "usage: $0 <build_dir> <version> [out.bin]"
build_dir=$(realpath -m "$1")
version=$2
bin=$build_dir/FPT_DMI_Embedded-IoT_Course/zephyr/zephyr.bin
out=$(realpath -m "${3:-$build_dir/upgrade-$version.bin}")
[ -r "$bin" ] || die "$bin not found: run scripts/build.sh first"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(\+[0-9]+)?$ ]] || die "bad version: $version"

key=${STM32H573_MCUBOOT_KEY_FILE:-$ws/bootloader/mcuboot/root-rsa-2048.pem}
[ -r "$key" ] || die "signing key not readable: $key"
use_venv

python "$ws/bootloader/mcuboot/scripts/imgtool.py" sign \
	--version "$version" --header-size 0x400 --slot-size 0xB0000 --align 16 \
	--pad --key "$key" "$bin" "$out"
echo "=== DONE: $out ($(stat -c %s "$out") bytes) ==="
echo "Flash to slot1 (0x080F0000), for example:"
echo "  STM32_Programmer_CLI -c port=SWD -d $out 0x080F0000 -rst"
