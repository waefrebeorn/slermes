#!/usr/bin/env python3
"""Honest build-health gate for Slermes.

Compiles EVERY tracked .c file with the real project flags and reports a
count. This exists because `make | grep error:` can report success while
building nothing -- the historical Makefile default goal was a single
SQLite3 object, so a bare `make` printed "up to date" and validated zero.

Usage:
    python tests/verify_build_health.py            # summary
    python tests/verify_build_health.py --list     # failing files
    python tests/verify_build_health.py --save     # write baseline JSON
    python tests/verify_build_health.py --regress  # exit 1 if worse than baseline
"""
import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASELINE = ROOT / "tests" / ".build_health_baseline.json"

CFLAGS = None
FLAGS_BY_FILE = {}


def real_cflags():
    """Use the project's own compile flags, harvested from `make -n`.

    Hand-rolled -I lists diverge from the real build and manufacture phantom
    errors (e.g. missing -I lib -> 'libdb/db.h: No such file'). Ask make.
    """
    r = subprocess.run(["make", "-n", "-B"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
    for line in r.stdout.splitlines():
        if " -c " in line and ".c" in line:
            flags = line.split(" -c ")[0]
            flags = flags.replace(str(ROOT).replace("\\", "/"), "")
            parts = flags.split()
            if parts and parts[0].endswith("gcc"):
                parts = parts[1:]
            # make prints -DNAME=\\\"value\\\" (shell-escaped). Passing that
            # straight to execve hands gcc a literal backslash, which it reports
            # as "stray '\'". Collapse the escaping back to a plain quote pair.
            parts = [re.sub(r'\\+"', '"', p) for p in parts]
            return parts + ["-fsyntax-only"]
    raise SystemExit("could not harvest compile flags from `make -n`")


CC = os.environ.get("CC", "gcc")


def build_sources():
    """Source files this platform's build actually compiles.

    Scanning every .c over-reports: window_wayland.c and xdg-shell-protocol.c
    only exist on Linux (objects.mk gates them on UNAME_S) and will never
    compile on Windows. Scope to what `make -n` would compile.
    """
    r = subprocess.run(["make", "-n", "-B"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
    # Keep make's command line verbatim and hand it to sh: the -D values are
    # shell-escaped (\"v\"), and re-parsing them into execve argv hands gcc a
    # literal backslash ('stray ' in program'). Shell evaluation is exact.
    out = {}
    for line in r.stdout.splitlines():
        if " -c -o " not in line:
            continue
        head, tail = line.split(" -c -o ", 1)
        src = tail.split()[-1]
        if not src.endswith(".c"):
            continue
        out.setdefault(src, head)
    return out


def sources():
    # Walk the tree instead of shelling out to git: this box's MSYS python
    # cannot spawn the native git.exe (WinError 5), and a filesystem scan has
    # no dependency on git state anyway.
    out = []
    for sub in ("src",):
        for p in (ROOT / sub).rglob("*.c"):
            out.append(p.relative_to(ROOT).as_posix())
    return sorted(out)


def check(path):
    cmd = FLAGS_BY_FILE.get(path)
    if cmd:
        shell_cmd = f'{cmd} -fsyntax-only "{path}"'
    else:
        shell_cmd = f'{CC} -fsyntax-only "{path}"'
    r = subprocess.run(
        ["sh", "-c", shell_cmd],
        cwd=ROOT, capture_output=True, text=True,
        encoding="utf-8", errors="replace",
    )
    if r.returncode == 0:
        return None
    n = sum(1 for ln in r.stderr.splitlines() if "error:" in ln)
    return {"file": path, "errors": n}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--save", action="store_true")
    ap.add_argument("--regress", action="store_true")
    ap.add_argument("--no-link", action="store_true",
                    help="skip the link check (sources only)")
    args = ap.parse_args()

    per_file = build_sources()
    files = sorted(per_file)
    global CFLAGS, FLAGS_BY_FILE
    FLAGS_BY_FILE = per_file
    CFLAGS = next(iter(per_file.values())) if per_file else real_cflags()
    with ThreadPoolExecutor(max_workers=8) as ex:
        results = [r for r in ex.map(check, files) if r]

    failing = sorted(results, key=lambda r: -r["errors"])
    total = len(files)
    bad = len(failing)

    print(f"Slermes build health: {total - bad}/{total} clean, {bad} failing")
    if bad:
        print(f"Total compiler errors across failures: {sum(r['errors'] for r in failing)}")
    if args.list:
        for r in failing:
            print(f"  {r['errors']:5d}  {r['file']}")

    if args.save:
        BASELINE.write_text(json.dumps(
            {"total": total, "failing": [r["file"] for r in failing]}, indent=1))
        print(f"baseline written: {len(failing)} failing")

    if args.regress:
        if not BASELINE.exists():
            print("no baseline; run --save first")
            return 1
        base = set(json.loads(BASELINE.read_text())["failing"])
        now = {r["file"] for r in failing}
        new = sorted(now - base)
        fixed = sorted(base - now)
        if new:
            print(f"\nREGRESSION: {len(new)} newly broken")
            for f in new:
                print(f"  + {f}")
            return 1
        print(f"\nNo regressions. Fixed {len(fixed)} since baseline.")
        for f in fixed:
            print(f"  - {f}")

    # ---- link check -------------------------------------------------------
    # -fsyntax-only proves a file parses. It does NOT prove the symbols resolve.
    # A duplicate definition or a missing object leaves every source "clean"
    # while the binary fails to link -- which is exactly how a compile-only
    # gate reported success against a broken build. Always link.
    if not args.no_link:
        print("\nLinking (make -j4 all) ...")
        r = subprocess.run(["make", "-j4", "all"], cwd=ROOT, capture_output=True,
                           text=True, encoding="utf-8", errors="replace")
        if r.returncode != 0:
            print("LINK FAILED")
            seen = set()
            for ln in (r.stdout + r.stderr).splitlines():
                if "error:" in ln or "multiple definition" in ln or "undefined reference" in ln:
                    key = ln.strip()[:160]
                    if key not in seen:
                        seen.add(key)
                        print(f"  {key}")
                        if len(seen) >= 12:
                            break
            return 1
        print("LINK OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
