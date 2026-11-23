#!/usr/bin/env python3
"""Differential oracle for gate w12_runtimeown_e8fe0a (gateway/runtime_ownership.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_runtimeown_e8fe0a"
sys.path.insert(0, QUARRY)

# make_user_dir is imported at module load but none of the ported seams use it.
_da = types.ModuleType("hermes_dir_acl")
_da.make_user_dir = lambda *a, **k: None
sys.modules["hermes_dir_acl"] = _da

SRC = QUARRY + "/gateway/runtime_ownership.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)

NS = types.SimpleNamespace


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def stat_of(v):
    v = v or {}
    return NS(st_dev=v.get("dev"), st_ino=v.get("ino"))


def compute(r):
    func = r.get("func")
    if func == "foreign_lock_recovery":
        return live.foreign_lock_recovery(r.get("path"))
    if func == "same_inode":
        return live._same_inode(stat_of(r.get("left")), stat_of(r.get("right")))
    if func == "canonical_home":
        return str(live.canonical_home(Path(r.get("home") or "")))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
