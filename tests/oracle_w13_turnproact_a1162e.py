#!/usr/bin/env python3
"""Differential oracle for agent/turn_proactive_prune.py (gate w13_turnproact_a1162e).

Drives the LIVE route_change_offload / offload_cold_history gates with scripted
clock/route/prune seams (the expensive prune helper is a dependency, not this
module's own decision).  Compares the returned verdict/messages to the C port.
"""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

Q = "C:/Users/eman5/hermes-agent"
G = "w13_turnproact_a1162e"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _seed(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + pn.replace(".", "/")]
            sys.modules[pn] = pm
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parent = ".".join(parts[:-1])
    if parent:
        setattr(sys.modules[parent], parts[-1], mod)
    return mod


_seed("agent")
_seed("agent.context_compressor", _DB_PERSISTED_MARKER="__db_persisted__")
_seed("agent.prompt_cache_clock", cache_window=lambda a: None,
      prompt_cache_is_cold=lambda a: False, _display_route=lambda a: "")
_seed("agent.turn_preflight", _proactive_tool_result_prune=lambda *a, **k: [])
_seed("agent.turn_context", _preflight_request_tokens=lambda *a, **k: 0)
cm = _seed("agent.conversation_compression_manual", estimate_request_tokens=lambda *a, **k: 0)

_spec = importlib.util.spec_from_file_location(G + "_live", Q + "/agent/turn_proactive_prune.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)

_FUNCS = ("route_offload", "offload_cold")


def live_out(rec):
    func = rec.get("func")
    messages = rec.get("messages") or []
    if func == "route_offload":
        wr = rec.get("window_reason")
        win = None if wr is None else types.SimpleNamespace(reason=wr)
        live.cache_window = lambda a: win
        sys.modules["agent.prompt_cache_clock"]._display_route = lambda a: rec.get("route") or ""
        sys.modules["agent.turn_context"]._preflight_request_tokens = lambda *a, **k: 0
        pruned = rec.get("pruned_messages") or []
        sys.modules["agent.turn_preflight"]._proactive_tool_result_prune = lambda *a, **k: pruned
        clock = object()  # unique identity marker for the `is` comparison
        previous = None
        if rec.get("has_previous"):
            previous = (clock if rec.get("same_clock") else object(), rec.get("prev_route") or "")
        agent = types.SimpleNamespace(
            compression_enabled=bool(rec.get("compression_enabled")),
            _prompt_cache_clock=clock,
            _route_offload_checked=previous,
            context_compressor=object())
        v = live.route_change_offload(
            agent, messages=messages,
            current_turn_user_idx=rec.get("current_turn_user_idx", 0),
            active_system_prompt=rec.get("active_system_prompt"),
            effective_task_id="task")
        return {"action": v.action, "messages": v.messages}
    if func == "offload_cold":
        live.prompt_cache_is_cold = lambda a: bool(rec.get("cache_cold"))
        cm.estimate_request_tokens = lambda *a, **k: 0
        pruned = rec.get("pruned_messages") or []
        if rec.get("pruned_same"):
            sys.modules["agent.turn_preflight"]._proactive_tool_result_prune = lambda a, c, m, t, **k: m
        else:
            sys.modules["agent.turn_preflight"]._proactive_tool_result_prune = lambda a, c, m, t, **k: pruned
        agent = types.SimpleNamespace(compression_enabled=bool(rec.get("compression_enabled")),
                                      context_compressor=object(), session_id="s")
        return live.offload_cold_history(agent, messages, "task")
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in _FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%s func=%s C=%r PY=%r" %
                  (G, rec.get("case"), rec.get("func"), got, py), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
    return 1 if (mism or cases == 0) else 0


if __name__ == "__main__":
    sys.exit(main())
