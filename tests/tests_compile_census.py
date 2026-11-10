#!/usr/bin/env python3
"""Compile census for tests/*.c.

Honest scope, stated plainly:

  * Include paths are the UNION of every -I flag the real build uses, harvested
    from `make -n -B`. Individual test harnesses also need specific .o files at
    link time, which this census does not attempt -- so a PASS here means
    "preprocesses and parses against the project's full header surface", not
    "links and passes".
  * A FAIL here is therefore a real first-order problem: the file cannot even
    be parsed with every project header available.

Earlier attempts at this number were wrong because they used a hand-picked
subset of -I directories and reported hundreds of "yaml.h not found" failures.
Never hand-pick include paths.

Usage: python tests/tests_compile_census.py [--list]
"""
import argparse
import pathlib
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = pathlib.Path(__file__).resolve().parent.parent


def union_includes():
    r = subprocess.run(["make", "-n", "-B"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
    dirs, defs = [], []
    for line in r.stdout.splitlines():
        if " -c -o " not in line:
            continue
        head = line.split(" -c -o ", 1)[0]
        toks = head.split()
        for i, tok in enumerate(toks):
            if tok == "-I" and i + 1 < len(toks):
                d = toks[i + 1]
                if d not in dirs:
                    dirs.append(d)
            elif tok.startswith("-I") and len(tok) > 2:
                d = tok[2:]
                if d not in dirs:
                    dirs.append(d)
    # Version defines, with plain quotes (make prints shell-escaped values).
    defs = ['-DHERMES_VERSION="0.15.1-slermes"',
            '-DHERMES_RELEASE_DATE="2026.5.29"',
            '-DATADIR="/share/slermes/docs"']
    return dirs, defs


def compile_one(path, flags):
    cmd = f'gcc {" ".join(flags)} -fsyntax-only "{path}"'
    r = subprocess.run(["sh", "-c", cmd], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
    if r.returncode == 0:
        return None
    n = sum(1 for ln in r.stderr.splitlines() if "error:" in ln)
    first = next((ln for ln in r.stderr.splitlines() if "error:" in ln), "")
    return {"file": path, "errors": n, "first": first}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    dirs, defs = union_includes()
    flags = ["-fsyntax-only-replaced"]  # placeholder, see below
    flags = []
    for d in dirs:
        flags += ["-I", d]
    flags += ["-include", "hermes_win32.h", "-w"] + defs

    files = sorted(p.relative_to(ROOT).as_posix()
                   for p in (ROOT / "tests").rglob("*.c"))
    with ThreadPoolExecutor(max_workers=8) as ex:
        results = [r for r in ex.map(lambda f: compile_one(f, flags), files) if r]

    bad = len(results)
    print(f"tests/*.c parse census: {len(files) - bad}/{len(files)} clean, {bad} failing")
    print(f"include dirs used: {len(dirs)}")
    if args.list:
        for r in sorted(results, key=lambda r: -r["errors"])[:60]:
            print(f"  {r['errors']:4d}  {r['file']}")
            if r["first"]:
                print(f"        {r['first'].strip()[:110]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
