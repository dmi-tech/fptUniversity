# Shared helpers, sourced by the other scripts (not run directly).
app_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
repo_dir=$(git -C "$app_dir" rev-parse --show-toplevel 2>/dev/null) || {
	echo "[ERR] $app_dir is not inside a git clone" >&2
	exit 1
}
ws=$(cd "$repo_dir/.." && pwd)
repo_name=$(basename "$repo_dir")
manifest_file=${app_dir#"$repo_dir"/}/west.yml

step() { echo; echo "=== $* ==="; }
die()  { echo "[ERR] $*" >&2; exit 1; }

# Put the workspace virtualenv first on PATH, if setup.sh created one and it
# still works (it breaks when the system Python is upgraded: run setup.sh again)
use_venv() {
	if [ -x "$ws/.venv/bin/python" ] && "$ws/.venv/bin/python" -c 'import west' 2>/dev/null; then
		export PATH="$ws/.venv/bin:$PATH"
	elif [ -e "$ws/.venv" ]; then
		echo "[WARN] $ws/.venv is broken (Python upgraded?). Run $app_dir/scripts/setup.sh to repair it." >&2
	fi
	command -v west >/dev/null || die "west not found. Run $app_dir/scripts/setup.sh first."
}
