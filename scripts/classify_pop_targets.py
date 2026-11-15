#!/usr/bin/env python3
"""Classify each PoP annotation whose module is absent.

Two very different situations look identical from a filename check:
  A) the module was renamed/moved  -> the annotation is stale, re-point it
  B) the ported function no longer exists anywhere -> orphan port, no oracle
                                     can ever verify it

Distinguishing them needs a search for the PYTHON FUNCTION NAME, not the file.
"""
import re
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
ROOTS = [REPO, Path("C:/Users/eman5/hermes-agent")]
POP = re.compile(r"PoP:\s*(\S+)\s*@\s*([A-Za-z0-9_./-]+\.py)\s*:\s*(\S+)")

# One pass: every identifier defined in any Python file in either tree.
defined = set()
for root in ROOTS:
    if not root.exists():
        continue
    for py in root.rglob("*.py"):
        if "/tests/" in py.as_posix() or "/.git/" in py.as_posix():
            continue
        try:
            txt = py.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        defined |= set(re.findall(r"^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)", txt, re.M))
        defined |= set(re.findall(r"^\s*class\s+([A-Za-z_][A-Za-z0-9_]*)", txt, re.M))
        defined |= set(re.findall(r"^([A-Za-z_][A-Za-z0-9_]*)\s*=", txt, re.M))

# Names so generic that "found elsewhere" means nothing -- `send` existing in
# some unrelated module does not make the annotation re-pointable.
GENERIC = {
    "send", "connect", "close", "start", "stop", "run", "init", "__init__",
    "format_message", "truncate_message", "_redact", "state", "reset",
    "handle", "process", "update", "get", "set", "load", "save", "main",
    "__enter__", "__exit__", "__str__", "__repr__", "call", "done", "flush",
}

missing_mod, orphan, stale = [], [], []
seen = set()
for c in list(REPO.glob("src/**/port_*.c")) + list(REPO.glob("src/**/*_ports.c")):
    txt = c.read_text(encoding="utf-8", errors="replace")
    for _cname, mod, pyname in POP.findall(txt):
        if (mod, pyname) in seen:
            continue
        seen.add((mod, pyname))
        if any((r / mod).exists() for r in ROOTS):
            continue
        missing_mod.append((mod, pyname))
        if pyname in defined and pyname not in GENERIC:
            stale.append((mod, pyname))       # distinctive name found elsewhere
        else:
            orphan.append((mod, pyname))      # nowhere distinctive: orphan port

print(f"distinct PoP targets with an absent module: {len(missing_mod)}")
print(f"  stale  -- function exists elsewhere, re-point the annotation: {len(stale)}")
print(f"  orphan -- function exists nowhere, no oracle can verify it: {len(orphan)}")
print()
print("ORPHAN modules:")
for m in sorted({m for m, _ in orphan}):
    n = sum(1 for mm, _ in orphan if mm == m)
    print(f"  {m:52} {n:3} functions")
print()
print("STALE samples (function found elsewhere):")
for m, f in stale[:10]:
    print(f"  {m} :: {f}")
