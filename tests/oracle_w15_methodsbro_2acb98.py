#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_browser.py (gate w15_methodsbro_2acb98).

Loads the LIVE module with a stubbed tui_gateway.method_ctx and injects
contextlib (the live module relies on server.py's rebound globals for it).
Compares _is_default_local_cdp(urlparse(url)) and sorted(_CDP_SCHEMES) with
the C harness `out`.
"""
import contextlib
import importlib.util
import json
import sys
import types
from urllib.parse import urlparse

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

tui = types.ModuleType("tui_gateway")
tui.__path__ = [QUARRY + "/tui_gateway"]
sys.modules["tui_gateway"] = tui
mctx = types.ModuleType("tui_gateway.method_ctx")
mctx.bind_module = lambda *a, **k: None
mctx.HandlerRegistry = object
sys.modules["tui_gateway.method_ctx"] = mctx

_spec = importlib.util.spec_from_file_location(
    "tui_gateway.methods_browser", QUARRY + "/tui_gateway/methods_browser.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["tui_gateway.methods_browser"] = live
_spec.loader.exec_module(live)
live.contextlib = contextlib

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "default_local":
            py = live._is_default_local_cdp(urlparse(r.get("url", "")))
        elif func == "schemes":
            py = sorted(live._CDP_SCHEMES)
        else:
            continue
    except Exception as e:
        print("PY-ERROR %r: %s" % (r, e), file=sys.stderr)
        mism += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w15_methodsbro_2acb98: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
