#!/usr/bin/env python3
"""Differential oracle for agent/turn_truncation.py (gate w7_turntruncat_79d683)."""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("agent")
_stub("agent.error_classifier", FailoverReason=SimpleNamespace())
_stub("agent.message_metadata", append_message=lambda *a, **k: None)
_stub("agent.message_sanitization", close_interrupted_tool_sequence=lambda *a, **k: None)
_stub("agent.repetition_guard", is_repetition_dominated=lambda t: False)
_stub("agent.turn_api_call", stop_thinking_spinner=lambda *a, **k: None)
_stub("agent.turn_failure_copy", content_policy_copy=lambda **k: "", provider_label_for=lambda p: str(p),
      site_copy=lambda key, **k: "<%s>" % key, stamp_failure=lambda d, *a, **k: d)
_stub("agent.turn_retry_state", TurnRetryState=object)
_stub("agent.usage_pricing", normalize_usage=lambda u, **k: SimpleNamespace(prompt_tokens=0))
_stub("hermes_constants", PARTIAL_STREAM_STUB_ID="__partial_stream_stub__")

LIVE_NAME = "w7_turntruncat_79d683_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/agent/turn_truncation.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def _i(v, default=0):
    return int(v) if v not in (None, "") else default


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
    if func == "repetition_copy":
        py = list(live.repetition_copy(r.get("stopping", ""), r.get("outcome", ""), r.get("refusal", "")))
    elif func == "partial_result":
        err = r.get("error") if r.get("has_error") else None
        py = live.partial_result([], _i(r.get("api_calls")), r.get("final_response", ""), err,
                                 failed=bool(r.get("failed")),
                                 compression_exhausted=bool(r.get("compression_exhausted")))
    elif func == "boosted_output_cap":
        hlim = bool(r.get("model_limit") is not None)
        limit = _i(r.get("model_limit")) if hlim else None
        _stub("agent.anthropic_adapter",
              _get_anthropic_max_output=lambda model: (limit if hlim else None))
        agent = SimpleNamespace(max_tokens=_i(r.get("max_tokens")) or None,
                                api_mode=r.get("api_mode", "chat_completions"), model="m")
        req = int(r["requested_cap"]) if r.get("requested_cap") is not None else None
        base = int(r["base"]) if r.get("base") is not None else None
        py = live.boosted_output_cap(agent, req, _i(r.get("n")), base=base)
        py = float(py)
    else:
        continue

    cases += 1
    got = r.get("out")
    if func == "boosted_output_cap" and isinstance(got, int):
        got = float(got)
    if py != got:
        mism += 1
        print("MISMATCH %s args=%r py=%r c=%r" % (func, r, py, got), file=sys.stderr)

print("ORACLE w7_turntruncat_79d683: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
