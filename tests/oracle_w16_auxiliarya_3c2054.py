#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_async_rebuild.py (gate w16_auxiliarya_3c2054)."""
import asyncio
import importlib.util
import inspect
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_auxiliarya_3c2054"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_oai = types.ModuleType("openai")
class OpenAI:
    pass
_oai.OpenAI = OpenAI
sys.modules["openai"] = _oai

_spec = importlib.util.spec_from_file_location(
    "agent.auxiliary_async_rebuild_live", QUARRY + "/agent/auxiliary_async_rebuild.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

class Plain:
    pass

def _client(spec):
    cls = OpenAI if spec.get("is_openai") else Plain
    c = cls()
    c.api_key = spec.get("api_key")
    if spec.get("headers") is not None:
        c._custom_headers = spec["headers"]
    if spec.get("provider") is not None:
        pv = spec["provider"]
        c._api_key_provider = lambda pv=pv: pv
    return c

def _compute(r):
    f = r.get("func")
    spec = r.get("client") or {}
    c = _client(spec)
    if f == "async_key":
        v = live.async_api_key(c)
        if inspect.iscoroutinefunction(v):
            v = asyncio.run(v())
        return v
    if f == "headers":
        return live.configured_default_headers(c)
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
