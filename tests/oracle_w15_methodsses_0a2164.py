#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_session.py (gate w15_methodsses_0a2164)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_methodsses_0a2164"
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
_mc = types.ModuleType("tui_gateway.method_ctx")
class _HR:
    def method(self, *a, **k):
        return lambda fn: fn
    def profile_scoped(self, fn):
        return fn
_mc.HandlerRegistry = _HR
_mc.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = _mc

# INTERNAL_LISTING_SOURCES is the real upstream value; inject it so the live
# frozenset _LISTING_DENY_SOURCES matches the C mirror.
for _name, _path in (("persistence", QUARRY + "/persistence"),
                     ("persistence.conversation", QUARRY + "/persistence/conversation")):
    _m = types.ModuleType(_name); _m.__path__ = [_path]; sys.modules[_name] = _m
_ps = types.ModuleType("persistence.conversation.sessions")
_ps.INTERNAL_LISTING_SOURCES = ("kanban", "tool", "oneshot")
sys.modules["persistence.conversation.sessions"] = _ps

_spec = importlib.util.spec_from_file_location(
    "tui_gateway.methods_session_live", QUARRY + "/tui_gateway/methods_session.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

def _compute(r):
    f = r.get("func")
    if f == "str_param":
        return live._str_param(r.get("params") or {}, r.get("key", ""), r.get("default", ""))
    if f == "int_param":
        return live._int_param(r.get("params") or {}, r.get("key", ""), r.get("default", 0))
    if f == "denied":
        return live._denied_source(r.get("row") or {})
    if f == "auto_denied":
        return live._auto_resume_denied_source(r.get("row") or {})
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
    try:
        py = _compute(r)
    except Exception as exc:
        print("PYERROR %r %r" % (r, exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
sys.exit(1 if mism or not cases else 0)
