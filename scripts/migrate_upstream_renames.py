#!/usr/bin/env python3
"""migrate_upstream_renames.py — retool slermes parity refs after an upstream
reorganization (checkpoint day 2026-11-18: pluggable-DB migration, 228 renames).

Reads the git rename map (old path -> new path) from the quarry and rewrites:
  1. PoP annotations in src/ + include/ + lib/  (@ old.py:fn -> @ new.py:fn)
  2. sta_oracle_*.py imports (dotted module paths)
  3. watercup gate.json py_module fields
  4. watercup gate DIRECTORY names (real__ / port__ / part__ prefixes encode
     the module path; the scanner's twin resolution depends on real__ names)
  5. scripts/pyport_map.json path references

Usage:
  python scripts/migrate_upstream_renames.py <old_rev> <new_rev> [--apply]
Without --apply it only reports what would change (dry run).
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = Path(os.environ.get("SLERMES_QUARRY", str(Path.home() / "hermes-agent")))
# WindowsApps blocks the bundled git; use the portable copy when present.
_pg = Path.home() / "AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/bin/git.exe"
GIT = str(_pg) if _pg.is_file() else "git"
if _pg.is_file():
    os.environ["GIT_EXEC_PATH"] = str(
        Path.home() / "AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/libexec/git-core")
APPLY = "--apply" in sys.argv
revs = [a for a in sys.argv[1:] if not a.startswith("--")]
OLD, NEW = revs[0], revs[1]


def rename_map():
    out = subprocess.run(
        [GIT, "diff", "--diff-filter=R", "--name-status", f"{OLD}..{NEW}", "--", "*.py"],
        cwd=QUARRY, capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
    m = {}
    for line in out.splitlines():
        parts = line.split("\t")
        if len(parts) == 3:
            m[parts[1].replace("\\", "/")] = parts[2].replace("\\", "/")
    return m


def dotted(p):
    return p[:-3].replace("/", ".") if p.endswith(".py") else p.replace("/", ".")


def gate_prefix(p):
    stem = p[:-3] if p.endswith(".py") else p
    return stem.replace("/", "_") + "_py___"


def main():
    rm = rename_map()
    print(f"rename map: {len(rm)} modules")
    # basename map for bare-name annotations; detect collisions
    base = {}
    collide = set()
    for old, new in rm.items():
        b = os.path.basename(old)
        if b in base and base[b] != new:
            collide.add(b)
        base[b] = new
    if collide:
        print("basename collisions (skipped for bare-name rewrite):", sorted(collide))

    stats = {"annotations": 0, "ann_files": 0, "oracle_imports": 0,
             "gate_json": 0, "gate_dirs": 0, "pyport_map": 0}

    # 1. PoP annotations
    ann_re = re.compile(r"(@\s*)([\w/.]+\.py)(\s*:)")
    for sub in ("src", "include", "lib"):
        for p in (REPO / sub).rglob("*"):
            if p.suffix not in (".c", ".h") or not p.is_file():
                continue
            try:
                t = p.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            hits = 0

            def sub_ann(m):
                nonlocal hits
                claim = m.group(2).replace("\\", "/")
                new = rm.get(claim)
                if new is None and "/" not in claim and claim not in collide:
                    new = base.get(claim)
                if new and new != claim:
                    hits += 1
                    return m.group(1) + new + m.group(3)
                return m.group(0)

            t2 = ann_re.sub(sub_ann, t)
            if hits:
                stats["annotations"] += hits
                stats["ann_files"] += 1
                if APPLY:
                    p.write_text(t2, encoding="utf-8", newline="\n")
    print(f"annotations: {stats['annotations']} in {stats['ann_files']} files")

    # 2. oracle imports
    for p in (REPO / "tests").glob("sta_oracle_*.py"):
        t = p.read_text(encoding="utf-8")
        hits = 0
        for old, new in rm.items():
            do, dn = dotted(old), dotted(new)
            pat = re.compile(r"\b(from|import)\s+" + re.escape(do) + r"\b")
            t, n = pat.subn(lambda m: m.group(1) + " " + dn, t)
            hits += n
        if hits:
            stats["oracle_imports"] += hits
            if APPLY:
                p.write_text(t, encoding="utf-8", newline="\n")
    print(f"oracle import lines: {stats['oracle_imports']}")

    # 3+4. watercup gates
    gates = REPO / "tests" / "watercup" / "gates"
    old_prefixes = {gate_prefix(o): (o, n) for o, n in rm.items()}
    for gd in sorted(gates.iterdir()):
        if not gd.is_dir():
            continue
        gj = gd / "gate.json"
        if gj.is_file():
            try:
                g = json.loads(gj.read_text(encoding="utf-8"))
            except Exception:
                g = None
            if g and g.get("py_module") in rm:
                stats["gate_json"] += 1
                if APPLY:
                    g["py_module"] = rm[g["py_module"]]
                    gj.write_text(json.dumps(g, indent=2, ensure_ascii=False) + "\n",
                                  encoding="utf-8", newline="\n")
        for pref, (o, n) in old_prefixes.items():
            for kind in ("real__", "port__", "part__"):
                if gd.name.startswith(kind + pref):
                    newname = kind + gate_prefix(n) + gd.name[len(kind + pref):]
                    stats["gate_dirs"] += 1
                    if APPLY:
                        target = gates / newname
                        if not target.exists():
                            gd.rename(target)
                    break
    print(f"gate.json py_module: {stats['gate_json']}, gate dirs: {stats['gate_dirs']}")

    # 5. pyport_map.json
    pm = REPO / "scripts" / "pyport_map.json"
    if pm.is_file():
        t = pm.read_text(encoding="utf-8")
        hits = 0
        for old, new in rm.items():
            if old in t:
                hits += t.count(old)
                t = t.replace(old, new)
        if hits:
            stats["pyport_map"] = hits
            if APPLY:
                pm.write_text(t, encoding="utf-8", newline="\n")
    print(f"pyport_map refs: {stats['pyport_map']}")
    print("APPLIED" if APPLY else "DRY RUN (pass --apply to write)")


if __name__ == "__main__":
    main()
