#!/usr/bin/env python3
"""Differential oracle for gate w11_turnfinal_45ceb6 (agent/turn_final_response.py).

The live module is imported with its agent-state collaborators stubbed in
sys.modules and driven by the fixture flags. Only the two leaf predicates that
need real gateway state (housekeeping_reply_line / is_intentional_silence_response)
are injected; every branch of the live functions under test still executes.
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
_mod("agent.message_metadata", append_message=lambda m, x: m.append(x))
_mod("agent.reasoning_promotion", answer_in_reasoning_capability=lambda a: True)
_mod("agent.repetition_guard", STOP_PATH_MIN_CHARS=200, is_runaway_repetition=lambda s: False)
_mod("agent.stream_delivery", publish_held_final_row=lambda *a, **k: None)
_mod("agent.turn_failure_copy", stamp_failure=lambda d, *a: d)
_mod("agent.turn_empty_response",
     housekeeping_reply_line=lambda a: R.get("reply"),
     recover_empty_response=lambda *a, **k: None,
     take_housekeeping_reply_line=lambda a: R.get("reply"))
_mod("agent.turn_stop_gates", apply_stop_gates=lambda *a, **k: None)
_mod("agent.turn_truncation", partial_result=lambda *a, **k: {},
     repetition_copy=lambda *a: (a[0], a[1], a[2]))

gateway_pkg = _mod("gateway")
gateway_pkg.__path__ = []
_silence_calls = {"n": 0}


def _silence(s):
    i = _silence_calls["n"]
    _silence_calls["n"] += 1
    return bool(R.get("reply_is_silence" if i == 0 else "final_is_silence", False))


_mod("gateway.response_filters", is_intentional_silence_response=_silence)

# Verbatim copy of agent.conversation_loop._continuation_overlap_length /
# _join_truncated_parts (the collaborator the live function calls).
_MIN_CONTINUATION_OVERLAP = 32


def _overlap(previous, continuation):
    if len(previous) < _MIN_CONTINUATION_OVERLAP or len(continuation) < _MIN_CONTINUATION_OVERLAP:
        return 0
    prefix_lengths = [0] * len(continuation)
    matched = 0
    for index in range(1, len(continuation)):
        while matched and continuation[index] != continuation[matched]:
            matched = prefix_lengths[matched - 1]
        if continuation[index] == continuation[matched]:
            matched += 1
            prefix_lengths[index] = matched
    matched = 0
    last_index = len(previous) - 1
    for index, char in enumerate(previous):
        while matched and char != continuation[matched]:
            matched = prefix_lengths[matched - 1]
        if char == continuation[matched]:
            matched += 1
            if matched == len(continuation):
                if index == last_index:
                    return matched
                matched = prefix_lengths[matched - 1]
    return matched if matched >= _MIN_CONTINUATION_OVERLAP else 0


def _join(parts):
    joined = ""
    previous_was_partial_stub = False
    for part, is_partial_stub in parts:
        if previous_was_partial_stub and joined and part:
            part = part[_overlap(joined[-len(part):], part):]
        if joined and not joined[-1].isspace() and part and not part[0].isspace():
            joined += "\n"
        joined += part
        previous_was_partial_stub = is_partial_stub
    return joined


_mod("agent.conversation_loop", _join_truncated_parts=_join)

SRC = QUARRY + "/agent/turn_final_response.py"
spec = importlib.util.spec_from_file_location("w11_turnfinal_45ceb6_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

_EPHEMERAL_FLAGS = (
    "_thinking_prefill", "_empty_recovery_synthetic", "_empty_terminal_sentinel",
    "_dropped_toolcall_nudge",
)

# `is not True` is an identity check: a JSON number 1 is NOT the Python True.
_NEEDS_REPLY = {"true": True, "false": False, "none": None, "one": 1, "str": "x"}


def compute(r):
    global R
    R = r
    f = r.get("func")
    if f == "bare_marker":
        _silence_calls["n"] = 0
        a = types.SimpleNamespace(
            _silence_marker_needs_reply=_NEEDS_REPLY.get(r.get("needs_reply", "false"), False),
            _interrupt_requested=bool(r.get("interrupt_requested", False)),
            _strip_think_blocks=lambda s: r.get("stripped_final", s))
        return bool(live._bare_marker_after_reply(
            a, r.get("final_response", ""),
            truncated_parts=bool(r.get("truncated_parts", False)),
            promoted=bool(r.get("promoted", False))))
    if f == "absorb":
        import copy
        parts = [(p["text"], bool(p["is_partial_stub"])) for p in r.get("parts", [])]
        msgs = copy.deepcopy(r.get("messages", []))
        joined, had = live._absorb_continuation_fragments(r.get("final_response", ""), parts, msgs)
        return {"joined": joined, "had_parts": bool(had), "messages": msgs}
    if f == "pop_scaffolding":
        import copy
        msgs = copy.deepcopy(r.get("messages", []))
        n = 0
        while msgs and isinstance(msgs[-1], dict) and any(msgs[-1].get(k) for k in _EPHEMERAL_FLAGS):
            msgs.pop()
            n += 1
        return {"count": n, "messages": msgs}
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
    print("ORACLE w11_turnfinal_45ceb6: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
