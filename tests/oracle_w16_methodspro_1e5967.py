#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_profiles.py (gate w16_methodspro_1e5967)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_methodspro_1e5967"
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
_mc.HandlerRegistry = _HR
_mc.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = _mc

# The live module imports this coercion helper at module load time.
_hc = types.ModuleType("hermes_cli")
_hc.__path__ = [QUARRY + "/hermes_cli"]
sys.modules["hermes_cli"] = _hc
_pc = types.ModuleType("hermes_cli.profiles_cache")
_pc.yaml_scalar_to_json = lambda v: v
sys.modules["hermes_cli.profiles_cache"] = _pc

_spec = importlib.util.spec_from_file_location(
    "tui_gateway.methods_profiles_live", QUARRY + "/tui_gateway/methods_profiles.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

def _compute(r):
    f = r.get("func")
    if f == "model_provider_params":
        return list(live._model_provider_params(r.get("params") or {}))
    if f == "clean_revisions":
        return live._clean_revisions(r.get("raw") or {})
    if f == "clean_names":
        return sorted(live._clean_names(r.get("values") or []))
    if f == "bots_title":
        return live._bots_title(r.get("meta") or {})
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
