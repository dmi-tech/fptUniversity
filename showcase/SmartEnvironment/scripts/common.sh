# Shared helpers, sourced by setup.sh / build.sh / flash.sh (not run directly).
#
# Sets: app_dir (this showcase), app_name, repo_dir (git clone), ws (west
# workspace: the directory that holds the clone), manifest_file (this
# showcase's west.yml, relative to the clone).
app_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
app_name=$(basename "$app_dir")
repo_dir=$(git -C "$app_dir" rev-parse --show-toplevel 2>/dev/null) || {
	echo "[ERR] $app_dir is not inside a git clone" >&2
	exit 1
}
ws=$(cd "$repo_dir/.." && pwd)
manifest_file=${app_dir#"$repo_dir"/}/west.yml

step() { echo; echo "=== $* ==="; }
die()  { echo "[ERR] $*" >&2; exit 2; }

# Use the workspace virtualenv created by setup.sh, if there is one
use_venv() {
	if [ -x "$ws/.venv/bin/west" ]; then
		export PATH="$ws/.venv/bin:$PATH"
	fi
	command -v west >/dev/null || die "west not found. Run $app_dir/scripts/setup.sh first."
}
