#!/usr/bin/env python3
"""Differential oracle for gate w11_turnrecove_ba5e00 (agent/turn_recovery_codex.py).

Live pure helpers run against deterministic injected response/error objects; the
C port mirrors them. classify_api_error and _extract_api_error_context are
stubbed with deterministic shapes driven by the fixture.
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
_mod("agent.error_classifier", classify_api_error=None, FailoverReason=types.SimpleNamespace(
    rate_limit="rate_limit", billing="billing"))
_mod("agent.route_verdicts", route_visit=lambda a: 1)
_mod("agent.turn_retry_state", TurnRetryState=type("TurnRetryState", (), {}))
_mod("agent.turn_recovery", _vlines=lambda *a, **k: None, validate_response_shape=lambda *a: (False, []))


def _classify_api_error(exc, *, provider, model, base_url, api_key):
    e = exc.body["error"]
    return {"code": e.get("code"), "message": e.get("message"),
            "provider": provider, "model": model, "base_url": base_url, "api_key": api_key}


sys.modules["agent.error_classifier"].classify_api_error = _classify_api_error

SRC = QUARRY + "/agent/turn_recovery_codex.py"
spec = importlib.util.spec_from_file_location("w11_turnrecove_ba5e00_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    global R
    R = r
    f = r.get("func")
    if f == "soft_error":
        resp = types.SimpleNamespace(error=r.get("error"))
        return live._codex_soft_failure_error(resp)
    if f == "token_expired":
        api_error = types.SimpleNamespace(status_code=r.get("status_code"))
        agent = types.SimpleNamespace(
            _extract_api_error_context=lambda e: {"reason": r.get("reason")})
        return bool(live._is_codex_token_expired(agent, api_error))
    if f == "classify":
        agent = types.SimpleNamespace(
            api_mode=r.get("api_mode"), provider=r.get("provider", "") or "",
            model=r.get("model", "") or "", base_url=r.get("base_url", "") or "",
            api_key=r.get("api_key"),
            _extract_api_error_context=lambda exc: {"error": exc.body["error"]})
        resp = types.SimpleNamespace(status=r.get("status"), error=r.get("error"))
        classified, ctx = live.classify_codex_soft_failure(agent, resp)
        return {"classified": classified, "error_context": ctx}
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
    print("ORACLE w11_turnrecove_ba5e00: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
