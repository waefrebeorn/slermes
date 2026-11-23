#!/usr/bin/env python3
"""Differential oracle for tui_gateway/server_requests.py (gate w15_serverrequ_cdbd30)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_serverrequ_cdbd30"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# The live module imports tools.clarify_batch at import time; inject a stub.
_tools = types.ModuleType("tools")
_tools.__path__ = [QUARRY + "/tools"]
sys.modules["tools"] = _tools
_cb = types.ModuleType("tools.clarify_batch")
_cb.settle_batch = lambda locked, result=None, ended=None: {"answers": locked, "outcome": ended or "submitted"}
sys.modules["tools.clarify_batch"] = _cb

_spec = importlib.util.spec_from_file_location(
    "server_requests_live", QUARRY + "/tui_gateway/server_requests.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

def _compute(r):
    f = r.get("func")
    if f == "can_replay":
        return live.can_replay_request(r.get("method", ""), r.get("params") or {})
    if f == "not_shown":
        return live._is_not_shown(r.get("frame") or {})
    if f == "is_response":
        return live.is_response_frame(r.get("obj"))
    return None

def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {k: _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or "{" not in line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = _compute(r)
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
sys.exit(1 if mism or not cases else 0)
