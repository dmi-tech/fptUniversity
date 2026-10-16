# One-time setup of the west workspace on Windows, for VS Code + the
# "IDE for Zephyr" extension (mylonics.zephyr-ide). Safe to run again.
# Windows counterpart of setup.sh.
#
# The workspace is the directory that holds the repository clone:
#
#   <workspace>\                  e.g. C:\zws (keep the path short)
#   +-- fptUniversity\            <- this repository
#   +-- .venv\  .west\            <- created here
#   +-- zephyr\  modules\  bootloader\
#   +-- .vscode\zephyr-ide.json   <- project + build for the extension
#
# Steps: check host tools, create .venv with west, west init + update, install
# the Python requirements, install the Zephyr SDK (ARM toolchain), write the
# extension configuration and the signing key setting.
#
# Usage (PowerShell, from the workspace):
#   powershell -ExecutionPolicy Bypass -File <project>\scripts\setup.ps1 [-KeyFile <key.pem>]
#
# -KeyFile (or STM32H573_MCUBOOT_KEY_FILE): RSA-2048 private key that signs the
# firmware. When none is given, the PUBLIC MCUboot development key is used (lab
# use only). Stored in <workspace>\.vscode\<project>-signing.conf; run again
# with another key to change it, or edit that file.
param([string]$KeyFile = $env:STM32H573_MCUBOOT_KEY_FILE)

$ErrorActionPreference = 'Stop'

# $true: without a key, sign with the public MCUboot development key (lab use
# only). $false: a key must be given before building.
$DevKeyFallback = $true

function Step($msg) { Write-Host ""; Write-Host "=== $msg ===" }
function Die($msg) { Write-Host "[ERR] $msg" -ForegroundColor Red; exit 1 }
function Warn($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }

# Run a native command and stop on a non-zero exit code
function Run {
	$exe = $args[0]
	$rest = @($args | Select-Object -Skip 1)
	& $exe @rest
	if ($LASTEXITCODE -ne 0) { Die "failed ($LASTEXITCODE): $($args -join ' ')" }
}

# Run a native command for its output only: no stderr, never stops the script
# (check $LASTEXITCODE)
function Quiet {
	$old = $ErrorActionPreference
	$ErrorActionPreference = 'Continue'
	try {
		$exe = $args[0]
		$rest = @($args | Select-Object -Skip 1)
		& $exe @rest 2>$null
	} finally {
		$ErrorActionPreference = $old
	}
}

function Has($cmd) { [bool](Get-Command $cmd -ErrorAction SilentlyContinue) }

$appDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$appName = Split-Path $appDir -Leaf
if (-not (Has git)) { Die "git not found (winget install Git.Git)" }
$repoDir = Quiet git -C $appDir rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or -not $repoDir) { Die "$appDir is not inside a git clone" }
$repoDir = (Resolve-Path $repoDir).Path
$repoName = Split-Path $repoDir -Leaf
$ws = Split-Path $repoDir -Parent
# Paths relative to the workspace, with / (west and the extension use them)
$relApp = $appDir.Substring($ws.Length).TrimStart('\').Replace('\', '/')
$manifestFile = $appDir.Substring($repoDir.Length).TrimStart('\').Replace('\', '/') + '/west.yml'

if ($ws.Length -le 3) {
	Die "the workspace would be $ws. Clone into an empty directory first, e.g. C:\zws"
}

Step "Checking host tools"
# Tools installed by winget are not on PATH until a new terminal is opened
foreach ($d in @("$env:ProgramFiles\7-Zip", "$env:ProgramFiles\CMake\bin")) {
	if ((Test-Path $d) -and ($env:Path -notlike "*$d*")) { $env:Path = "$d;$env:Path" }
}
$missing = @()
foreach ($t in @('cmake', 'ninja', 'dtc', '7z')) {
	if (-not (Has $t)) { $missing += $t }
}
$py = $null
foreach ($cand in @('py -3', 'python')) {
	$c = $cand.Split(' ')
	if (Has $c[0]) {
		Quiet @c -c "import sys, venv; sys.exit(sys.version_info < (3, 12))" | Out-Null
		if ($LASTEXITCODE -eq 0) { $py = $c; break }
	}
}
if (-not $py) { $missing += 'python (3.12+)' }
if ($missing.Count -gt 0) {
	Write-Host "[ERR] missing: $($missing -join ', ')" -ForegroundColor Red
	Write-Host "      Install them with the Host Tools page of IDE for Zephyr, or:"
	Write-Host "      winget install Kitware.CMake Ninja-build.Ninja oss-winget.dtc Python.Python.3.12 7zip.7zip"
	Write-Host "      then open a new terminal and run this script again."
	exit 1
}
if ((Quiet git config --global core.longpaths) -ne 'true') {
	Warn "git core.longpaths is off: west update may fail on long paths."
	Write-Host "       git config --global core.longpaths true"
}
Write-Host "OK"

Step "Python environment ($ws\.venv)"
$venvPy = "$ws\.venv\Scripts\python.exe"
if (Test-Path "$ws\.venv") {
	Quiet $venvPy -c "import west" | Out-Null
	if ($LASTEXITCODE -ne 0) {
		$old = "$ws\.venv.broken-$(Get-Date -Format yyyyMMdd-HHmmss)"
		Warn "$ws\.venv is unusable (Python upgraded?). Moving it to $old"
		Move-Item "$ws\.venv" $old
	}
}
if (-not (Test-Path $venvPy)) {
	Run @py -m venv "$ws\.venv"
}
$env:Path = "$ws\.venv\Scripts;$env:Path"
Run $venvPy -m pip install -q --upgrade pip west

Step "west workspace ($ws)"
Push-Location $ws
try {
	if (Test-Path "$ws\.west") {
		$curPath = Quiet west config manifest.path
		$curFile = Quiet west config manifest.file
		if ($curPath -ne $repoName) {
			Die "$ws\.west belongs to another repository ($curPath/$curFile)"
		}
		if ($curFile -ne $manifestFile) {
			# Only a sibling project's manifest (<dir>/west.yml) may be replaced
			if ($curFile -notlike '*/west.yml') {
				Die "$ws\.west uses the repository manifest '$curFile'; refusing to replace it"
			}
			Write-Host "switching manifest: $curFile -> $manifestFile"
			Run west config manifest.file $manifestFile
		} else {
			Write-Host "already initialised"
		}
	} else {
		Run west init -l --mf $manifestFile $repoName
	}
	Run west update --narrow -o=--depth=1

	Step "Python requirements"
	Run $venvPy -m pip install -q -r "$ws\zephyr\scripts\requirements.txt"
	Run $venvPy -m pip install -q -r "$ws\bootloader\mcuboot\scripts\requirements.txt"
	Write-Host "OK"

	Step "Zephyr SDK (arm-zephyr-eabi)"
	# Uses the version in zephyr\SDK_VERSION; skipped when already installed
	Run west sdk install -t arm-zephyr-eabi
} finally {
	Pop-Location
}

Step "Flash tool"
$cubeCli = @(
	"${env:ProgramFiles}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
	"${env:ProgramFiles(x86)}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
) | Where-Object { Test-Path $_ }
if ((Has STM32_Programmer_CLI) -or $cubeCli) {
	Write-Host "STM32CubeProgrammer found"
} else {
	Warn "STM32CubeProgrammer not found: building works, flashing does not."
	Write-Host "       Install it from https://www.st.com/en/development-tools/stm32cubeprog.html"
}

Step "Signing key"
$vscodeDir = "$ws\.vscode"
New-Item -ItemType Directory -Force $vscodeDir | Out-Null
$signConf = "$vscodeDir\$appName-signing.conf"
$utf8 = New-Object System.Text.UTF8Encoding $false
$keyPath = $null
if ($KeyFile) {
	if (-not (Test-Path $KeyFile)) { Die "key file not found: $KeyFile" }
	$keyPath = (Resolve-Path $KeyFile).Path
} elseif (Test-Path $signConf) {
	Write-Host "keeping $signConf"
} elseif ($DevKeyFallback) {
	$keyPath = "$ws\bootloader\mcuboot\root-rsa-2048.pem"
	Warn "no key given: signing with the public MCUboot development key. Lab use only."
} else {
	$keyPath = 'C:\keys\showcase-rsa2048.pem'
	Warn "no key given. Put the RSA-2048 private key at $keyPath,"
	Write-Host "       or run again with -KeyFile <key.pem>, or edit $signConf"
}
if ($keyPath) {
	# Sysbuild Kconfig fragment: the key path needs literal quotes, which the
	# extension's CMake arguments cannot carry
	$text = "# Written by $appName\scripts\setup.ps1: firmware signing key (MCUboot)`n" +
		"SB_CONFIG_BOOT_SIGNATURE_KEY_FILE=`"$($keyPath.Replace('\', '/'))`"`n"
	[IO.File]::WriteAllText($signConf, $text, $utf8)
	Write-Host "key: $keyPath ($signConf)"
}

Step "IDE for Zephyr configuration ($vscodeDir\zephyr-ide.json)"
# One project with one build "build" (output in <project>\build, as build.sh).
# The entries of other projects in the file are kept.
$cfgFile = "$vscodeDir\zephyr-ide.json"
$cfg = $null
if (Test-Path $cfgFile) {
	$cfg = Get-Content $cfgFile -Raw | ConvertFrom-Json
}
if (-not $cfg) { $cfg = [pscustomobject]@{} }
if (-not $cfg.PSObject.Properties['projects']) {
	$cfg | Add-Member -NotePropertyName projects -NotePropertyValue ([pscustomobject]@{})
}
$noFiles = [ordered]@{ config = @(); overlay = @() }
$project = [ordered]@{
	name           = $appName
	relPath        = $relApp
	buildConfigs   = [ordered]@{
		build = [ordered]@{
			name               = 'build'
			board              = 'stm32h573ri_custom'
			relBoardDir        = "$relApp/boards"
			relBoardSubDir     = 'st/stm32h573ri_custom'
			westBuildArgs      = @('--sysbuild')
			westBuildCMakeArgs = @("-DSB_EXTRA_CONF_FILE=$($signConf.Replace('\', '/'))")
			confFiles          = $noFiles
		}
	}
	confFiles      = $noFiles
	twisterConfigs = [ordered]@{}
}
$cfg.projects | Add-Member -Force -NotePropertyName $appName -NotePropertyValue $project
$toolchains = @()
if ($cfg.PSObject.Properties['toolchains']) { $toolchains = @($cfg.toolchains) }
if ($toolchains -notcontains 'arm-zephyr-eabi') { $toolchains += 'arm-zephyr-eabi' }
$cfg | Add-Member -Force -NotePropertyName toolchains -NotePropertyValue $toolchains
[IO.File]::WriteAllText($cfgFile, ($cfg | ConvertTo-Json -Depth 20), $utf8)
Write-Host "project $appName ($relApp), build 'build'"

Write-Host ""
Write-Host "=== Setup done. Next, in VS Code: ==="
Write-Host "  1. File > Open Folder: $ws"
Write-Host "  2. Ctrl+Shift+P > Zephyr IDE: Setup Workspace from Current Directory > Use .west folder"
Write-Host "  3. Select project $appName, build 'build', then Build / Flash"
