#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/redact_hermes_home.py.

The live guard is exercised with controlled stubs: os.path primitives are
pure Windows-lexical string functions (no host filesystem), _hermes_dirs()
returns the fixture's home list, and sandbox_lexical_path() is None (local,
non-hosted scope). The C port implements the identical normalisation.
"""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
B = "\\"
SRC = Q + "/agent/redact_hermes_home.py"
spec = importlib.util.spec_from_file_location("redhome_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

import agent.file_safety as fs
import tools.environments.sandbox_scope as ss

def win_norm(p):
    p = str(p).replace("/", B)
    segs = []
    for x in p.split(B):
        if x == "" or x == ".":
            continue
        if x == "..":
            if segs and not segs[-1].endswith(":"):
                segs.pop()
            continue
        segs.append(x)
    return B.join(segs)

def normcase(p):
    return str(p).replace("/", B).lower()

class FakePath:
    def __init__(self, s): self.s = s
    def __str__(self): return self.s

HOMES = []
def _homes(): return [FakePath(h) for h in HOMES]
fs._hermes_dirs = _homes
ss.sandbox_lexical_path = lambda p: None
fake_os = types.SimpleNamespace(path=types.SimpleNamespace(
    normcase=normcase, realpath=win_norm, expanduser=lambda p: str(p)), sep=B)
live.os = fake_os

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func"); path = r.get("path", "")
    if func in ("is_under", "is_under_ex"):
        HOMES[:] = r.get("homes", [])
        py = live._is_under_hermes_home(path)
    elif func == "win_norm":
        py = win_norm(path)
    elif func == "normcase":
        py = normcase(path)
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE redhome: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
