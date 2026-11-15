#!/usr/bin/env python3
"""Rename decidable unit-owned headers to port_<python module>.h.

Decidable = the unit owns exactly one include/ header AND the target name is not
already owned by another unit. Multi-header units and colliding targets are
reported by tests/verify_naming.py as undecidable and left alone.

Refuses to run if the plan is not collision-free.
"""
import collections
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
GIT = "C:/Users/eman5/AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/bin/git.exe"
GIT_ENV = ("C:/Users/eman5/AppData/Local/hermes/tools/git-2.53.0-win32-x64/"
           "mingw64/libexec/git-core")
POP = re.compile(r"PoP:\s+(\S+)\s+@\s+([\w./]+\.py):")
INC = re.compile(r'#\s*include\s*[<"]([^>"]+)[>"]')

users = collections.defaultdict(set)
texts = {}
unit_rels = set()
for c in sorted(REPO.glob("src/**/port_*.c")) + sorted(REPO.glob("src/**/*_ports.c")):
    t = c.read_text(encoding="utf-8", errors="replace")
    if not POP.search(t):
        continue
    rel = c.relative_to(REPO).as_posix()
    texts[rel] = t
    unit_rels.add(rel)
# ALL includers count, not just port units -- otherwise a header shared with a
# non-unit file looks owned and gets renamed out from under its other callers.
for base in ("src", "include", "tests", "lib"):
    for p in (REPO / base).rglob("*"):
        if p.suffix not in (".c", ".h"):
            continue
        try:
            t = p.read_bytes().decode("utf-8", errors="replace")
        except OSError:
            continue
        for inc in INC.findall(t):
            if (REPO / "include" / inc).exists():
                users[inc].add(p.relative_to(REPO).as_posix())

owned = {}
for inc, rels in users.items():
    if len(rels) == 1 and next(iter(rels)) in unit_rels:
        owned.setdefault(next(iter(rels)), []).append(Path(inc).name)

taken = {h for hdrs in owned.values() for h in hdrs}

plan = []
for rel, hdrs in owned.items():
    if len(hdrs) != 1:
        continue
    m = POP.search(texts[rel])
    if not m:
        continue
    target = "port_" + Path(m.group(2)).stem + ".h"
    if hdrs[0] == target or target in taken:
        continue
    plan.append((rel, hdrs[0], target))

# collision check across the plan itself
targets = collections.Counter(new for _, _, new in plan)
# Targets claimed by two units (platform adapters all port a module called
# adapter.py) are undecidable -- drop them, leave them for the gate to report.
bad = [t for t, n in targets.items() if n > 1]
dropped = []
if bad:
    keep = []
    for rel, old, new in plan:
        if new in bad:
            dropped.append((rel, old, new))
        else:
            keep.append((rel, old, new))
    plan = keep
    print(f"dropped {len(dropped)} colliding renames (undecidable):")
    for rel, old, new in dropped:
        print(f"    {old:38} -> {new:38} ({rel})")

print(f"decidable renames: {len(plan)}")

files = [p for base in ("src", "include", "tests", "lib")
         for p in (REPO / base).rglob("*") if p.suffix in (".c", ".h")]
inc_re = re.compile(r'(#\s*include\s*[<"])([^>"]+)([>"])')
renames = {old: new for _, old, new in plan}
subs = touched = 0
for p in files:
    text = p.read_bytes().decode("utf-8")
    orig = text

    def sub(m):
        global subs
        name = Path(m.group(2)).name
        new = renames.get(name)
        if new:
            subs += 1
            return m.group(1) + m.group(2).replace(name, new) + m.group(3)
        return m.group(0)

    text = inc_re.sub(sub, text)
    if text != orig:
        p.write_bytes(text.encode("utf-8"))
        touched += 1
print(f"include sites rewritten: {subs} in {touched} files")

env = dict(os.environ)
env["GIT_EXEC_PATH"] = GIT_ENV
moved = 0
for _, old, new in plan:
    src = REPO / "include" / old
    dst = REPO / "include" / new
    if not src.exists() or dst.exists():
        continue
    r = subprocess.run([GIT, "mv", f"include/{old}", f"include/{new}"],
                       cwd=REPO, env=env, capture_output=True, text=True)
    if r.returncode == 0:
        moved += 1
    else:
        print(f"  mv FAILED {old}: {r.stderr.strip()}")
print(f"headers moved: {moved}")

stale = 0
for p in files:
    if not p.exists():
        continue
    for m in inc_re.finditer(p.read_bytes().decode("utf-8", errors="replace")):
        if Path(m.group(2)).name in renames:
            stale += 1
print(f"stale includes: {stale}")
sys.exit(1 if stale else 0)
