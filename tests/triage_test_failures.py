#!/usr/bin/env python3
"""Triage tests/*.c compile failures into actionable classes.

For each failing test, pull the first 'implicit declaration of function X'
and ask whether a C definition exists whose name is a SCREAMING/normalised
match or a suffix match of X. Class A (exists under another name) is a cheap
rename/alias fix; Class B (absent) is real implementation work.

Usage: python tests/triage_test_failures.py [--limit N]
"""
import argparse
import pathlib
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = pathlib.Path(__file__).resolve().parent.parent
IMPLICIT = re.compile(r"implicit declaration of function '(\w+)'")
NO_MEMBER = re.compile(r"has no member named '(\w+)'")
UNKNOWN_TYPE = re.compile(r"unknown type name '(\w+)'")


def union_flags():
    r = subprocess.run(["make", "-n", "-B"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
    dirs = []
    for line in r.stdout.splitlines():
        if " -c -o " not in line:
            continue
        toks = line.split(" -c -o ", 1)[0].split()
        for i, tok in enumerate(toks):
            d = tok[2:] if tok.startswith("-I") and len(tok) > 2 else (
                toks[i + 1] if tok == "-I" and i + 1 < len(toks) else None)
            if d and d not in dirs:
                dirs.append(d)
    flags = []
    for d in dirs:
        flags += ["-I", d]
    return flags + ["-include", "hermes_win32.h", "-w",
                    '-DHERMES_VERSION="0.15.1-slermes"',
                    '-DHERMES_RELEASE_DATE="2026.5.29"',
                    '-DATADIR="/share/slermes/docs"']


def all_c_names():
    names = set()
    pat = re.compile(r"\b(\w+)\s*\([^;{)]*\)\s*\{")
    for p in list(ROOT.glob("src/**/*.c")) + list(ROOT.glob("lib/**/*.c")):
        try:
            txt = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        names.update(pat.findall(txt))
    return names


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def check(path, flags):
    cmd = f'gcc {" ".join(flags)} -fsyntax-only "{path}"'
    r = subprocess.run(["sh", "-c", cmd], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
    if r.returncode == 0:
        return None
    err = r.stderr
    m = IMPLICIT.search(err)
    missing = m.group(1) if m else None
    if not missing:
        m = UNKNOWN_TYPE.search(err)
        missing = m.group(1) if m else None
    if not missing:
        m = NO_MEMBER.search(err)
        missing = (m.group(1) if m else None)
    return {"file": path, "symbol": missing,
            "errors": err.count("error:")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=40)
    args = ap.parse_args()

    flags = union_flags()
    files = sorted(p.relative_to(ROOT).as_posix()
                   for p in (ROOT / "tests").rglob("*.c"))
    with ThreadPoolExecutor(max_workers=8) as ex:
        results = [r for r in ex.map(lambda f: check(f, flags), files) if r]

    names = all_c_names()
    by_norm = {}
    for n in names:
        by_norm.setdefault(norm(n), n)

    class_a, class_b = [], []
    for r in results:
        sym = r["symbol"]
        if not sym:
            continue
        target = by_norm.get(norm(sym))
        if not target:
            # suffix match: python_missing vs python_prefix_missing
            for n in names:
                if norm(n).endswith(norm(sym)) and len(norm(n)) > len(norm(sym)):
                    target = n
                    break
        (class_a if target else class_b).append((r, target))

    print(f"tests/*.c failing: {len(results)}")
    print(f"  Class A (symbol exists under another name): {len(class_a)}")
    print(f"  Class B (symbol absent, needs implementing): {len(class_b)}")
    print()
    print("Class A — cheap alias/rename fixes:")
    for r, t in class_a[:args.limit]:
        print(f"  {r['file']:52s} {r['symbol']:34s} -> {t}")
    print()
    print("Class B — real work:")
    for r, _ in class_b[:args.limit]:
        print(f"  {r['errors']:4d}  {r['file']:52s} {r['symbol']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
