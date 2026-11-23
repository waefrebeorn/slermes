#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_display.py (gate w15_methodsdis_df8f9b)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_methodsdis_df8f9b"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Parent-package bootstrap + a minimal method_ctx so the live module imports without
# pulling in the whole server; @method / @profile_scoped become identity decorators.
_tg = types.ModuleType("tui_gateway")
_tg.__path__ = [QUARRY + "/tui_gateway"]
sys.modules["tui_gateway"] = _tg
_mc = types.ModuleType("tui_gateway.method_ctx")
class _HandlerRegistry:
    def method(self, *a, **k):
        return lambda fn: fn
    def profile_scoped(self, fn):
        return fn
_mc.HandlerRegistry = _HandlerRegistry
_mc.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = _mc

_spec = importlib.util.spec_from_file_location(
    "tui_gateway.methods_display_live", QUARRY + "/tui_gateway/methods_display.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

class _Unweakable:           # WeakKeyDictionary.setdefault() raises TypeError here
    __slots__ = ()

class _Err(Exception):
    pass

def _wire(r):
    mine = list(r.get("mine") or [])
    live._unweakable_minted_ids = set(mine)
    live.current_transport = lambda: _Unweakable()
    live._err = lambda rid, code, msg, data=None: {"code": code, "message": msg, "data": data}

def _compute(r):
    f = r.get("func")
    _wire(r)
    if f == "mint":
        fake = types.ModuleType("secrets")
        fake.token_urlsafe = lambda n: r.get("generated", "")
        old = sys.modules.get("secrets")
        sys.modules["secrets"] = fake
        try:
            vid = live._mint_viewer_id(str(r.get("requested") or ""))
        finally:
            if old is None:
                sys.modules.pop("secrets", None)
            else:
                sys.modules["secrets"] = old
        return {"id": vid, "minted": sorted(live._unweakable_minted_ids)}
    if f == "foreign":
        foreign = live._foreign_viewer_id("rid", str(r.get("viewer_id") or "")) is not None
        return foreign
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
