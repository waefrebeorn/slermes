#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_managed_local.py (gate w16_auxiliarym_09359b)."""
import importlib.util
import json
import sys
from urllib.parse import urlparse

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_auxiliarym_09359b"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_spec = importlib.util.spec_from_file_location(
    "agent.auxiliary_managed_local_live", QUARRY + "/agent/auxiliary_managed_local.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

def _netloc(url):
    try:
        return urlparse(str(url)).netloc.lower()
    except ValueError:
        return ""

def _compute(r):
    f = r.get("func")
    if f == "netloc":
        return _netloc(r.get("base_url"))
    if f == "managed":
        managed = r.get("managed")
        live._managed_local_netloc = lambda m=managed: m
        return live._is_managed_local_endpoint(r.get("base_url"))
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
