#!/usr/bin/env bash
# Build the example main.c of every driver README (prompt 9.3). Never flashes.
#
# Usage: tests/readme_examples/run.sh [driver ...]     (default: all)
# Output: $BUILD_ROOT (default: tests/readme_examples/out)
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
app_dir=$(cd "$here/../.." && pwd)
source "$app_dir/scripts/common.sh"
use_venv

exec python3 -I "$here/build_examples.py" "$app_dir" "$ws" "${BUILD_ROOT:-$here/out}" "$@"
