#!/usr/bin/env python3
"""Differential oracle for gate w10_promptturn_a4cf0e (tui_gateway/prompt_turn.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# parent-package bootstrap + in-package relative import stub
class HandlerRegistry(object):
    def __init__(self, *a, **k): pass
def bind_module(*a, **k): return None
_mc = types.ModuleType("tui_gateway.method_ctx")
_mc.HandlerRegistry = HandlerRegistry
_mc.bind_module = bind_module
_tg = types.ModuleType("tui_gateway")
_tg.__path__ = [os.path.join(Q, "tui_gateway")]
_tg.method_ctx = _mc
sys.modules["tui_gateway"] = _tg
sys.modules["tui_gateway.method_ctx"] = _mc

_ag = types.ModuleType("agent"); _ag.__path__ = [os.path.join(Q, "agent")]
sys.modules["agent"] = _ag
_tfc = types.ModuleType("agent.turn_failure_copy")
_HOLDER = {"handoff": False}
_tfc.is_max_iteration_handoff = lambda result: _HOLDER["handoff"]
sys.modules["agent.turn_failure_copy"] = _tfc

NAME = "tui_gateway.prompt_turn_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "tui_gateway", "prompt_turn.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "result_status":
        return live._result_status(r.get("result") or {})
    if f == "is_successful_goal_turn":
        _HOLDER["handoff"] = bool(r.get("handoff", False))
        return live._is_successful_goal_turn(r.get("result"), r.get("status", ""), r.get("raw"))
    if f == "turn_outcome":
        live.turn_error_text = lambda e, s: r.get("error_text", "")
        live.INTERRUPT_WAITING_FOR_MODEL_PREFIX = r.get("interrupt_prefix", "")
        return live._turn_outcome(r.get("result"), r.get("error_surface"))
    return None

def canon(v):
    if isinstance(v, tuple):
        return [canon(x) for x in v]
    if isinstance(v, list):
        return [canon(x) for x in v]
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    return v

n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = canon(compute(r)), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_promptturn_a4cf0e: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
