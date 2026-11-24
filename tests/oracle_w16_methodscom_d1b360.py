#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_complete.py (gate w16_methodscom_d1b360)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_methodscom_d1b360"
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
class _HR:
    def method(self, *a, **k):
        return lambda fn: fn
    def profile_scoped(self, fn):
        return fn
_mc = types.ModuleType("tui_gateway.method_ctx")
_mc.HandlerRegistry = _HR
_mc.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = _mc

_spec = importlib.util.spec_from_file_location(
    "tui_gateway.methods_complete_live", QUARRY + "/tui_gateway/methods_complete.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

def _compute(r):
    f = r.get("func")
    if f == "item":
        return live._item(r.get("text"), r.get("meta"), r.get("display"))
    if f == "at_hints":
        return [list(x) for x in live._AT_DIRECTIVE_HINTS]
    if f == "builtin_prefixes":
        return sorted(live._BUILTIN_AT_PREFIXES)
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
