#!/usr/bin/env python3
"""Build the example main.c of every driver README, exactly as a student would.

For each driver/*/*/README.md it takes
  - the prj.conf block of "Bước 1", the first dts block of "Bước 2" and the cmake block of
    "Bước 3" (section "3. Cấu hình"),
  - the first ```c block of the section whose title contains "Ví dụ",
and builds them in a generated app (app only, no sysbuild, nothing is flashed).

Usage: build_examples.py <app_dir> <workspace> <build_root> [readme-name ...] [-j N]
Run it through run.sh (it sets up west).
"""
import concurrent.futures
import os
import re
import subprocess
import sys
from pathlib import Path

BASE_CONF = """CONFIG_PRINTK=y
CONFIG_CONSOLE=y
CONFIG_USE_SEGGER_RTT=y
CONFIG_RTT_CONSOLE=y
CONFIG_UART_CONSOLE=n
"""

# READMEs that say "as driver X" or "add the lines of driver Y": what the text asks for.
# conf_from / overlay_from: README whose block is part of this one's.
# extra_*: configuration of drivers the example uses in addition (named in the README text).
EXTRAS = {
    "services/mqtt": {
        "conf_from": ["devices/ethernet"],
        "overlay_from": ["devices/ethernet"],
        "extra_conf": "CONFIG_SENSOR=y\nCONFIG_SHT4X=y\nCONFIG_GPIO=y\nCONFIG_CBPRINTF_FP_SUPPORT=y\n",
        "extra_overlay": "/ { aliases { out0 = &user_led; }; };\n",
        "extra_cmake": ["devices/sht41", "peripherals/gpio"],
    },
}

FENCE = re.compile(r"^```(\w*)\s*$")


def blocks(lines):
    """Yield (lang, text, start_line) of every fenced block."""
    i = 0
    while i < len(lines):
        m = FENCE.match(lines[i])
        if m:
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            yield m.group(1), "\n".join(lines[i + 1:j]) + "\n", i
            i = j
        i += 1


def find_line(lines, pattern, start=0):
    for i in range(start, len(lines)):
        if re.search(pattern, lines[i]):
            return i
    return None


def parse(readme: Path):
    lines = readme.read_text().splitlines()
    out = {"conf": "", "overlay": "", "cmake": "", "main": None}

    s3 = find_line(lines, r"^## 3\.")
    s4 = find_line(lines, r"^## 4\.", s3 or 0)
    b1 = find_line(lines, r"^(###|\*\*)\s*(Chế độ.*)?.*Bước 1", s3 or 0)
    b2 = find_line(lines, r"Bước 2", b1 or 0)
    b3 = find_line(lines, r"Bước 3", b2 or 0)
    # Mode A of the motor: bold step headers come after "### Chế độ A"
    for lang, text, at in blocks(lines[: s4 or len(lines)]):
        if b1 is not None and b1 < at < (b2 or at + 1) and lang == "" and not out["conf"]:
            out["conf"] = text
        elif b2 is not None and b2 < at < (b3 or at + 1) and lang == "dts" and not out["overlay"]:
            out["overlay"] = text
        elif b3 is not None and b3 < at and lang == "cmake" and not out["cmake"]:
            out["cmake"] = text

    ex = find_line(lines, r"^## .*Ví dụ")
    if ex is not None:
        for lang, text, at in blocks(lines):
            if at > ex and lang == "c":
                out["main"] = text
                break
    return out


def conf_lines(text):
    return "".join(line + "\n" for line in text.splitlines() if line.startswith("CONFIG_"))


def build_one(name, driver_dir, app_dir, ws, build_root, parsed):
    p = parsed[name]
    if p["main"] is None:
        return name, "SKIP", "no ```c example"
    ex = EXTRAS.get(name, {})
    conf = BASE_CONF + conf_lines(p["conf"])
    overlay = p["overlay"]
    cmake = p["cmake"]
    for dep in ex.get("conf_from", []):
        conf += conf_lines(parsed[dep]["conf"])
    for dep in ex.get("overlay_from", []):
        overlay = parsed[dep]["overlay"] + overlay
    conf += ex.get("extra_conf", "")
    overlay += ex.get("extra_overlay", "")
    for dep in ex.get("extra_cmake", []):
        cmake += parsed[dep]["cmake"]

    gen = Path(build_root) / "apps" / name.replace("/", "_")
    (gen / "src").mkdir(parents=True, exist_ok=True)
    (gen / "src" / "main.c").write_text(p["main"])
    (gen / "prj.conf").write_text(conf)
    (gen / "app.overlay").write_text(overlay)
    (gen / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20.0)\n"
        "find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})\n"
        "project(example)\n"
        f"set(DRIVER_DIR {driver_dir})\n"
        "target_sources(app PRIVATE src/main.c)\n" + cmake
    )

    bdir = Path(build_root) / ("build-" + name.replace("/", "_"))
    log = Path(build_root) / ("build-" + name.replace("/", "_") + ".log")
    with open(log, "w") as lf:
        r = subprocess.run(
            ["west", "build", "-p", "always", "-d", str(bdir), "-b", "stm32h573ri_custom",
             str(gen), "--", f"-DBOARD_ROOT={app_dir}"],
            cwd=ws, stdout=lf, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        return name, "FAIL", str(log)
    text = log.read_text()
    warns = [l for l in text.splitlines()
             if re.search(r"(warning:|error:)", l) and "legacy_support" not in l
             and "MCUBOOT" not in l and "Wno" not in l and "-W" not in l.split("warning:")[0][-3:]]
    warns = [l for l in warns if not l.startswith("/usr/bin/ccache")]
    return name, "WARN" if warns else "PASS", (warns[0][:200] if warns else "")


def main():
    args = sys.argv[1:]
    jobs = 4
    if "-j" in args:
        k = args.index("-j")
        jobs = int(args[k + 1])
        del args[k:k + 2]
    app_dir, ws, build_root, *only = args
    driver_dir = Path(app_dir) / "driver"
    Path(build_root).mkdir(parents=True, exist_ok=True)

    parsed = {}
    for readme in sorted(driver_dir.glob("*/*/README.md")):
        parsed[f"{readme.parent.parent.name}/{readme.parent.name}"] = parse(readme)
    names = [n for n in parsed if not only or n.split("/")[1] in only or n in only]

    fail = 0
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        futs = [pool.submit(build_one, n, driver_dir, app_dir, ws, build_root, parsed) for n in names]
        for f in futs:
            name, status, info = f.result()
            print(f"{status:5} {name} {info}", flush=True)
            fail += status in ("FAIL", "WARN")
    print(f"{len(names)} examples, {fail} not clean")
    sys.exit(1 if fail else 0)


if __name__ == "__main__":
    main()
