#!/usr/bin/env bash
# One-time setup of the west workspace for fw_showcase. Safe to run again.
#
# The workspace is the directory that holds the repository clone:
#
#   <workspace>/
#   ├── fptUniversity/          <- this repository
#   │   └── fw_showcase/
#   ├── .venv/  .west/          <- created here
#   └── zephyr/  modules/  bootloader/
#
# Steps: check host tools, create .venv with west, west init + update,
# install the Python requirements, install the Zephyr SDK (ARM toolchain).
# The MCUboot signing key is NOT created; set STM32H573_MCUBOOT_KEY_FILE.
#
# Usage: fw_showcase/scripts/setup.sh
set -euo pipefail
app_dir=$(cd "$(dirname "$0")/.." && pwd)
repo_dir=$(git -C "$app_dir" rev-parse --show-toplevel 2>/dev/null) || {
	echo "[ERR] $app_dir is not inside a git clone" >&2
	exit 1
}
ws=$(cd "$repo_dir/.." && pwd)
repo_name=$(basename "$repo_dir")
manifest_file=${app_dir#"$repo_dir"/}/west.yml

step() { echo; echo "=== $* ==="; }
die()  { echo "[ERR] $*" >&2; exit 1; }

# Zephyr, the modules and .venv are created next to the clone
if [ "$ws" = "$HOME" ] || [ "$ws" = "/" ]; then
	die "the workspace would be $ws. Clone into an empty directory first:
      mkdir fw_showcase-workspace && cd fw_showcase-workspace && git clone <url>"
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
if [ ! -x "$ws/.venv/bin/python" ]; then
	python3 -m venv "$ws/.venv"
fi
export PATH="$ws/.venv/bin:$PATH"
pip install -q --upgrade pip west

step "west workspace ($ws)"
if [ -d "$ws/.west" ]; then
	cur_path=$(cd "$ws" && west config manifest.path 2>/dev/null || true)
	cur_file=$(cd "$ws" && west config manifest.file 2>/dev/null || true)
	if [ "$cur_path" != "$repo_name" ] || [ "$cur_file" != "$manifest_file" ]; then
		die "$ws/.west belongs to another manifest ($cur_path/$cur_file)"
	fi
	echo "already initialised"
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
	echo "[WARN] STM32H573_MCUBOOT_KEY_FILE is not set. Point it to the project's"
	echo "       RSA-2048 private key before building, e.g."
	echo "         export STM32H573_MCUBOOT_KEY_FILE=~/keys/fw_showcase-rsa2048.pem"
fi

echo
echo "=== Setup done. Next: ==="
echo "  $app_dir/scripts/build.sh     # build"
echo "  $app_dir/scripts/flash.sh     # build + flash over ST-LINK"
