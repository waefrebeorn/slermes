#!/usr/bin/env python3
"""Differential oracle for plugins/plugin_loader.py (w8_pluginloade_73daa6 gate)."""
import importlib._bootstrap
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/plugins/plugin_loader.py"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
if "plugins" not in sys.modules:
    pkg = types.ModuleType("plugins")
    pkg.__path__ = [QUARRY + "/plugins"]
    sys.modules["plugins"] = pkg

spec = importlib.util.spec_from_file_location("pluginloader_w8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pluginloader_w8_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r["func"]
    if f == "child_dir":
        p = live.plugin_child_dir(Path(r.get("root", "")), r.get("name"))
        return None if p is None else str(p).replace("\\", "/")
    if f == "deadlock":
        g = r.get("graph") or {}
        live._LOAD_OWNERS = {k: int(v) for k, v in (g.get("owners") or {}).items()}
        live._LOAD_WAITERS = {int(k): v for k, v in (g.get("waiters") or {}).items()}
        blocking = {}
        for k, v in (g.get("blocking") or {}).items():
            if isinstance(v, list):
                blocking[int(k)] = [types.SimpleNamespace(owner=int(x)) for x in v]
            else:
                blocking[int(k)] = types.SimpleNamespace(owner=int(v))
        importlib._bootstrap._blocking_on = blocking
        return live._load_would_deadlock(int(g.get("me", 0)))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE pluginloader: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH pluginloader {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE pluginloader: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
