#!/usr/bin/env python python3
"""Symbol index + minimal object closure for oracle harnesses.

Linking every harness against all 1,231 objects costs ~19 s per port, which is
why the suite takes ~13 min. Almost all of those objects are irrelevant to any
given harness.

This builds, once, an index of defined-symbol -> object, then for a harness
computes the minimal transitive closure: start from the harness object's
undefined symbols, add the objects that define them, repeat for the newly
added objects, stop when nothing new appears.

Usage:
  sym_index.py build [objects...]   -> write tests/oracle/build/sym_index.json
  sym_index.py closure <index> <harness.o> [extra.o ...]

The index is cached; delete it to rebuild.
"""
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
IDX = REPO / "tests/oracle/build/sym_index.json"


def defined_symbols(obj):
    """T-defined symbols in one object (global text/data/bss/undef excluded)."""
    try:
        out = subprocess.run(["nm", obj], capture_output=True, text=True,
                             timeout=60).stdout
    except Exception:
        return []
    syms = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] in ("T", "D", "B", "R", "C", "G", "S"):
            syms.append(parts[2])
        elif len(parts) == 2 and parts[0] in ("T", "D", "B", "R", "C"):
            syms.append(parts[1])
    return syms


def build(objs):
    index = {}
    with ThreadPoolExecutor(max_workers=12) as ex:
        for obj, syms in zip(objs, ex.map(defined_symbols, objs)):
            for s in syms:
                index.setdefault(s, []).append(obj)
    IDX.write_text(json.dumps(index), encoding="utf-8")
    print(f"index: {len(index)} symbols from {len(objs)} objects")


def undefined_symbols(obj):
    try:
        out = subprocess.run(["nm", obj], capture_output=True, text=True,
                             timeout=60).stdout
    except Exception:
        return set()
    u = set()
    for line in out.splitlines():
        parts = line.split()
        if parts and parts[0] == "U":
            u.add(parts[1])
        elif len(parts) == 2 and parts[1] == "U":
            u.add(parts[0])
    return u


def closure(index_path, seeds):
    index = json.loads(Path(index_path).read_text(encoding="utf-8"))
    chosen = []
    seen = set()
    pending = list(seeds)
    missing = set()
    while pending:
        obj = pending.pop()
        if obj in seen:
            continue
        seen.add(obj)
        for sym in undefined_symbols(obj):
            defs = index.get(sym)
            if defs:
                for d in defs:
                    if d not in seen:
                        pending.append(d)
            else:
                missing.add(sym)
    # The seeds themselves are needed too, but callers pass them separately.
    return sorted(seen), sorted(missing)


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "build":
        args = sys.argv[2:]
        objs = []
        for a in args:
            if a.startswith("@"):
                objs.extend(l.strip() for l in Path(a[1:]).read_text().splitlines()
                            if l.strip())
            else:
                objs.append(a)
        build(objs)
    elif cmd == "closure":
        got, missing = closure(sys.argv[2], sys.argv[3:])
        print(json.dumps({"objects": got, "missing": missing}))
    else:
        print(__doc__)
        sys.exit(2)
