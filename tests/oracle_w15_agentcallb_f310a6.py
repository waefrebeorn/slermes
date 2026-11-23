#!/usr/bin/env python3
"""Differential oracle for tui_gateway/agent_callbacks.py (gate w15_agentcallb_f310a6).

Loads the LIVE module with a stubbed tui_gateway.method_ctx and compares the
three ported data transforms with the C harness `out`:
  _parse_tui_skills_env (env read injected through os.environ),
  _preview_tool_result_preview, _preview_restart_history.
"""
import importlib.util
import json
import os
import sys
import types

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
    "tui_gateway.agent_callbacks", QUARRY + "/tui_gateway/agent_callbacks.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["tui_gateway.agent_callbacks"] = live
_spec.loader.exec_module(live)
# The live module references os.environ in _parse_tui_skills_env without
# importing os at module scope (it relies on server.py's rebound globals).
live.os = os

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
        if func == "skills":
            os.environ["HERMES_TUI_SKILLS"] = r.get("raw", "")
            py = live._parse_tui_skills_env()
        elif func == "preview":
            py = live._preview_tool_result_preview(r.get("name", ""), r.get("result", ""))
        elif func == "history":
            py = live._preview_restart_history(r.get("messages"),
                                               r.get("max_messages", 24),
                                               r.get("max_tool_chars", 1200))
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

print("ORACLE w15_agentcallb_f310a6: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
