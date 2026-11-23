#!/usr/bin/env python3
"""Differential oracle for gate w11_turniterat_f63754 (agent/turn_iteration_prep.py).

Live pure helpers run against fixture-driven injected constants; the C port
mirrors the same logic. Only lazily-imported placeholder constants and the
user_originated_turn_view collaborator are stubbed.
"""
import importlib.util
import json
import os
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

R = {}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent_pkg = _mod("agent")
agent_pkg.__path__ = []
_mod("agent.display", KawaiiSpinner=type("KawaiiSpinner", (), {}))
_mod("agent.interrupt_control",
     interrupt_issuer=lambda a: "sys",
     interrupted_during_api_call_reason=lambda a: "interrupted_during_api_call")
_mod("agent.turn_context_compaction", _reanchor=lambda a, m, u: 0)
_mod("agent.turn_truncation", boosted_output_cap=lambda a, c, n: c + n)
sr_pkg = _mod("session_rules")
sr_pkg.__path__ = []
_mod("session_rules.turns", user_originated_turn_view=lambda m: R.get("view"))
_mod("agent.agent_runtime_helpers_placeholders",
     _INTERRUPTED_PLACEHOLDER="", _LEGACY_INTERRUPTED_PLACEHOLDER="")
_mod("agent.conversation_loop", _INTERRUPT_SCAFFOLD_MARKER="")

SRC = QUARRY + "/agent/turn_iteration_prep.py"
spec = importlib.util.spec_from_file_location("w11_turniterat_f63754_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    global R
    R = r
    f = r.get("func")
    if f == "anchors":
        raw = r.get("idx", "0")
        idx = {"true": True, "false": False}.get(raw, int(raw) if str(raw).lstrip("-").isdigit() else raw)
        return bool(live._anchors_current_turn(r.get("messages", []), idx, r.get("user_message")))
    if f == "previous_tool_round":
        return live._previous_tool_round(r.get("messages", []))
    if f == "neutralise":
        ph = sys.modules["agent.agent_runtime_helpers_placeholders"]
        ph._INTERRUPTED_PLACEHOLDER = r.get("current_placeholder", "")
        ph._LEGACY_INTERRUPTED_PLACEHOLDER = r.get("legacy_placeholder", "")
        cl = sys.modules["agent.conversation_loop"]
        cl._INTERRUPT_SCAFFOLD_MARKER = r.get("scaffold_marker", "")
        msgs, n = live._neutralise_replay_echo_ghosts(r.get("seq", []))
        return {"messages": msgs, "neutralised": n}
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or "out" not in r:
            continue
        cases += 1
        try:
            py = compute(r)
        except Exception as exc:
            mism += 1
            print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)
    print("ORACLE w11_turniterat_f63754: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
