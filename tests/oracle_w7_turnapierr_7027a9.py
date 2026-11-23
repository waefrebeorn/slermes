#!/usr/bin/env python3
"""Differential oracle for agent/turn_api_error.py (gate w7_turnapierr_7027a9).

The live module's orchestration imports are stubbed in sys.modules; the real
_is_local_validation_error body from the live file is executed for every case.
is_provider_stream_parse_error is an injected collaborator driven by the
fixture's `stream_parse` key (the C seam mirrors that injection).
"""
import importlib.util
import json
import ssl
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent = _mod("agent")
agent.__path__ = []
# is_provider_stream_parse_error collaborator: returns the per-case injected flag.
aes = _mod("agent.api_error_summary", is_provider_stream_parse_error=lambda e: bool(getattr(aes, "_stream_parse", False)))
_mod("agent.error_classifier",
     RETRYABLE_CLIENT_REASONS=frozenset(), FailoverReason=types.SimpleNamespace(),
     classify_api_error=lambda *a, **k: None)
_mod("agent.route_verdicts", route_visit=lambda *a, **k: None)
_mod("agent.turn_overflow", recover_from_overflow=lambda *a, **k: None)
_mod("agent.turn_recovery",
     _NONRETRYABLE_LABELS={}, abort_turn_on_interrupt=lambda *a, **k: None,
     compute_error_backoff=lambda *a, **k: 0,
     free_tier_cooldown_ends_turn=lambda *a, **k: False,
     interruptible_backoff_sleep=lambda *a, **k: None,
     log_api_error_attempt=lambda *a, **k: (),
     max_retries_exhausted_result=lambda *a, **k: {},
     nonretryable_client_error_result=lambda *a, **k: {},
     recover_after_classification=lambda *a, **k: (False, None),
     recover_before_classification=lambda *a, **k: (False, None),
     route_classified_error=lambda *a, **k: None,
     settle_delivered_partial=lambda *a, **k: None)

SRC = QUARRY + "/agent/turn_api_error.py"
spec = importlib.util.spec_from_file_location("w7_turnapierr_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def make_exc(kind, message):
    if kind == "ValueError":
        return ValueError(message)
    if kind == "TypeError":
        return TypeError(message)
    if kind == "UnicodeEncodeError":
        return UnicodeEncodeError("utf-8", "x", 0, 1, "reason")
    if kind == "JSONDecodeError":
        return json.JSONDecodeError(message, "doc", 0)
    if kind == "SSLError":
        return ssl.SSLError(message)
    if kind == "RuntimeError":
        return RuntimeError(message)
    if kind == "KeyError":
        return KeyError(message)
    if kind == "OSError":
        return OSError(message)
    raise ValueError("unsupported exc_type: %r" % kind)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "is_local_validation_error":
        continue
    cases += 1
    aes._stream_parse = bool(r.get("stream_parse", False))
    try:
        exc = make_exc(r.get("exc_type", "ValueError"), r.get("message", ""))
        py = bool(live._is_local_validation_error(exc))
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %r" % (exc,), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s py=%r c=%r in=%s" % (r.get("case"), py, got, r), file=sys.stderr)

print("ORACLE w7_turnapierr_7027a9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
