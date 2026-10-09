#!/usr/bin/env bash
# One-time setup of the west workspace for EmbeddedCourse_FPT_DMI. Safe to run again.
#
# The workspace is the directory that holds the repository clone:
#
#   <workspace>/
#   ├── fptUniversity/          <- this repository
#   │   └── EmbeddedCourse_FPT_DMI/
#   ├── .venv/  .west/          <- created here
#   └── zephyr/  modules/  bootloader/
#
# Steps: check host tools, create (or repair) .venv with west, west init + update,
# install the Python requirements, install the Zephyr SDK (ARM toolchain).
# The workspace may already be set up for fw_showcase (same repository): the
# manifest is then switched to this project's west.yml.
#
# Usage: EmbeddedCourse_FPT_DMI/scripts/setup.sh
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

# Zephyr, the modules and .venv are created next to the clone
if [ "$ws" = "$HOME" ] || [ "$ws" = "/" ]; then
	die "the workspace would be $ws. Clone into an empty directory first:
      mkdir course-workspace && cd course-workspace && git clone <url>"
fi

step "Checking host tools"
missing=0
for t in git cmake ninja python3; do
	if ! command -v "$t" >/dev/null; then
		echo "[ERR] $t not found"
		missing=1
	fi
done
if command -v python3 >/dev/null && ! python3 -c 'import venv, ensurepip' 2>/dev/null; then
	echo "[ERR] python3 venv support not found (Ubuntu/Debian: python3-venv)"
	missing=1
fi
if command -v cmake >/dev/null; then
	v=$(cmake --version | head -1 | awk '{print $3}')
	if [ "$(printf '%s\n3.20.0\n' "$v" | sort -V | head -1)" != "3.20.0" ]; then
		echo "[ERR] cmake $v is too old, 3.20 or newer is needed"
		missing=1
	fi
fi
if [ "$missing" -ne 0 ]; then
	echo "      Ubuntu/Debian: sudo apt install git cmake ninja-build python3 python3-venv" >&2
	exit 1
fi
echo "OK"

step "Python environment ($ws/.venv)"
# A venv breaks when the system Python is upgraded (its python3 symlink then
# points to the new version and the installed packages are no longer found).
# Keep the old one aside and create a fresh venv.
if [ -e "$ws/.venv" ] && ! "$ws/.venv/bin/python" -c 'import west' 2>/dev/null; then
	old="$ws/.venv.broken-$(date +%Y%m%d-%H%M%S)"
	echo "[WARN] $ws/.venv is unusable (Python upgraded?). Moving it to $old"
	mv "$ws/.venv" "$old"
fi
if [ ! -x "$ws/.venv/bin/python" ]; then
	python3 -m venv "$ws/.venv"
fi
export PATH="$ws/.venv/bin:$PATH"
pip install -q --upgrade pip west

step "west workspace ($ws)"
if [ -d "$ws/.west" ]; then
	cur_path=$(cd "$ws" && west config manifest.path 2>/dev/null || true)
	cur_file=$(cd "$ws" && west config manifest.file 2>/dev/null || true)
	if [ "$cur_path" != "$repo_name" ]; then
		die "$ws/.west belongs to another repository ($cur_path/$cur_file)"
	fi
	if [ "$cur_file" != "$manifest_file" ]; then
		# Only a sibling project's manifest (<dir>/west.yml) may be replaced;
		# a repository-level west.yml belongs to a different setup.
		case "$cur_file" in
		*/west.yml) ;;
		*) die "$ws/.west uses the repository manifest '$cur_file'; refusing to replace it" ;;
		esac
		echo "switching manifest: $cur_file -> $manifest_file"
		(cd "$ws" && west config manifest.file "$manifest_file")
	else
		echo "already initialised"
	fi
else
	(cd "$ws" && west init -l --mf "$manifest_file" "$repo_name")
fi
(cd "$ws" && west update --narrow -o=--depth=1)

step "Python requirements"
pip install -q -r "$ws/zephyr/scripts/requirements.txt"
pip install -q -r "$ws/bootloader/mcuboot/scripts/requirements.txt"
echo "OK"

step "Zephyr SDK (arm-zephyr-eabi)"
# Uses the version in zephyr/SDK_VERSION; skipped when already installed
(cd "$ws" && west sdk install -t arm-zephyr-eabi)

step "Flash tool"
if command -v STM32_Programmer_CLI >/dev/null; then
	echo "STM32_Programmer_CLI found"
else
	echo "[WARN] STM32_Programmer_CLI not on PATH: building works, flashing does not."
	echo "       Install STM32CubeProgrammer (https://www.st.com/en/development-tools/stm32cubeprog.html)"
	echo "       and add its bin/ directory to PATH."
fi

step "Signing key"
if [ -n "${STM32H573_MCUBOOT_KEY_FILE:-}" ] && [ -r "$STM32H573_MCUBOOT_KEY_FILE" ]; then
	echo "STM32H573_MCUBOOT_KEY_FILE = $STM32H573_MCUBOOT_KEY_FILE"
else
	echo "[WARN] STM32H573_MCUBOOT_KEY_FILE is not set: build.sh will use the public"
	echo "       MCUboot development key (lab use only, never for products)."
fi

echo
echo "=== Setup done. Next: ==="
echo "  $app_dir/scripts/setup_udev.sh   # once, Linux: USB access for ST-LINK / J-Link"
echo "  $app_dir/scripts/build.sh        # build"
echo "  $app_dir/scripts/flash.sh        # build + flash over ST-LINK"
echo "  $app_dir/scripts/rtt.sh          # read the log (SEGGER RTT)"
