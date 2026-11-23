#!/usr/bin/env python3
"""Differential oracle for tui_gateway/session_workdir.py (gate w15_sessionwor_7524ea)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_sessionwor_7524ea"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_tg = types.ModuleType("tui_gateway")
_tg.__path__ = [QUARRY + "/tui_gateway"]
sys.modules["tui_gateway"] = _tg
_gp = types.ModuleType("tui_gateway.git_probe")
_tg.git_probe = _gp
sys.modules["tui_gateway.git_probe"] = _gp
_mc = types.ModuleType("tui_gateway.method_ctx")
_mc.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = _mc

_spec = importlib.util.spec_from_file_location(
    "tui_gateway.session_workdir_live", QUARRY + "/tui_gateway/session_workdir.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

def _compute(r):
    f = r.get("func")
    if f == "container_path":
        return live._is_container_path(r.get("raw", ""))
    if f == "valid_generation":
        return live._workdir_valid_generation(r.get("generation"))
    if f == "policy_backend":
        return live._policy_backend(r.get("policy") or {})
    if f == "session_source":
        live._resolve_session_platform = lambda: r.get("fallback", "local")
        return live._session_source(r.get("session"))
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
