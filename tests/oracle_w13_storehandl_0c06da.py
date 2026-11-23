#!/usr/bin/env python3
"""Differential oracle for w13_storehandl_0c06da (tui_gateway/store_handles.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/tui_gateway/store_handles.py"
sys.path.insert(0, QUARRY)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [QUARRY + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg

spec = importlib.util.spec_from_file_location("tui_gateway.store_handles", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.store_handles"] = live
spec.loader.exec_module(live)


class Persistence:
    def __init__(self, scope):
        self.scope = scope


def open_persistence(scope):
    return Persistence(scope)


pa = types.ModuleType("persistence_assembly")
pa.Persistence = Persistence
pa.open_persistence = open_persistence
sys.modules["persistence_assembly"] = pa


def proj(h, launch):
    if h is None:
        return None
    if h is launch:
        return {"handle": "launch"}
    return {"handle": "own", "scope": h.scope}


def compute(r):
    lai = bool(r.get("launch_is_handle"))
    launch = Persistence("launch") if lai else object()
    live._db = launch
    dbspec = r.get("db")
    if dbspec is None:
        db = None
    elif dbspec.get("is_launch"):
        db = launch
    elif dbspec.get("is_handle"):
        db = Persistence(dbspec.get("scope") or "")
    else:
        db = object()
    f = r.get("func")
    if f == "is_handle":
        return live._is_handle(db)
    if f == "launch_persistence":
        return proj(live._launch_persistence(), launch)
    if f == "persistence_beside":
        return proj(live._persistence_beside(db), launch)
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
        print(f"ORACLE w13_storehandl_0c06da: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w13_storehandl_0c06da {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_storehandl_0c06da: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
