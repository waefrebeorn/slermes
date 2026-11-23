#!/usr/bin/env python3
"""Differential oracle for gate w11_turnrespon_df1eeb (agent/turn_response_check.py).

Live finish-reason helpers run against deterministic injected responses; the C
port mirrors the same status/incomplete_details and transport decision tree.
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
_mod("agent.error_classifier", FailoverReason=types.SimpleNamespace(rate_limit="rate_limit"))
_mod("agent.prompt_cache_lifecycle", start_prompt_cache_warming=lambda *a, **k: None)
_mod("agent.turn_api_call", stop_thinking_spinner=lambda a, s: s)
_mod("agent.turn_failure_copy",
     invalid_response_failure_reason=lambda r: "invalid", provider_label_for=lambda p: p,
     site_copy=lambda *a, **k: {}, stamp_failure=lambda d, *a: d)
_mod("agent.turn_truncation",
     handle_content_policy_refusal=lambda *a, **k: types.SimpleNamespace(action="break", result=None),
     recover_from_truncation=lambda *a, **k: types.SimpleNamespace(action="fallthrough", result=None))
_mod("agent.turn_usage", record_response_usage=lambda *a, **k: None,
     note_response_reasoning_tokens=lambda *a, **k: None)
_mod("agent.turn_recovery", validate_response_shape=lambda *a: (False, []))

SRC = QUARRY + "/agent/turn_response_check.py"
spec = importlib.util.spec_from_file_location("w11_turnrespon_df1eeb_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _response(r):
    return types.SimpleNamespace(status=r.get("status"),
                                 incomplete_details=r.get("incomplete_details"))


def compute(r):
    global R
    R = r
    f = r.get("func")
    if f == "codex_finish_reason":
        return live._codex_finish_reason(_response(r))
    if f == "derive_finish_reason":
        transport = types.SimpleNamespace(
            normalize_response=lambda resp: types.SimpleNamespace(
                finish_reason=r.get("transport_finish", "stop"),
                tool_calls=bool(r.get("codex_tool_calls", False))),
            response_finish_reason=lambda resp: r.get("transport_finish", "stop"))
        agent = types.SimpleNamespace(
            api_mode=r.get("api_mode", "chat_completions"),
            _get_transport=lambda: transport,
            _should_treat_stop_as_truncated=lambda fr, n, m: bool(r.get("treat_stop_truncated", False)),
            log_prefix="", _vprint=lambda *a, **k: None)
        return live._derive_finish_reason(agent, _response(r), [])
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
    print("ORACLE w11_turnrespon_df1eeb: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
