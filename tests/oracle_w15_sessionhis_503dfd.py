#!/usr/bin/env python3
"""Differential oracle for tui_gateway/session_history.py (gate w15_sessionhis_503dfd)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_sessionhis_503dfd"
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
_mc.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = _mc
_ag = types.ModuleType("agent")
_ag.__path__ = [QUARRY + "/agent"]
sys.modules["agent"] = _ag
_pb = types.ModuleType("agent.prompt_builder")
_pb.STEER_DISPLAY_KIND = "steer"
sys.modules["agent.prompt_builder"] = _pb

_spec = importlib.util.spec_from_file_location(
    "tui_gateway.session_history_live", QUARRY + "/tui_gateway/session_history.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

def _compute(r):
    f = r.get("func")
    if f == "image_url":
        return live._history_part_image_url(r.get("part") or {})
    if f == "dict_text":
        return live._history_dict_text(r.get("content") or {}, image_urls=bool(r.get("image_urls", True)))
    if f == "display_text":
        return live._content_display_text(r.get("content"))
    if f == "coerce":
        return live._coerce_message_text(r.get("content"), image_urls=bool(r.get("image_urls", True)))
    if f == "hidden":
        return live._is_display_hidden_marker(r.get("role"), r.get("text", ""))
    if f == "strip_screenshots":
        s, n = live._strip_trailing_screenshot_placeholders(r.get("text", ""))
        return {"text": s, "count": n}
    if f == "seed_history":
        return live._coerce_seed_history(r.get("value"))
    if f == "inflight":
        return live._inflight_text(r.get("value"))
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
