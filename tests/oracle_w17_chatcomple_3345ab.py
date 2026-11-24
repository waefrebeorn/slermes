#!/usr/bin/env python3
"""Differential oracle for agent/chat_completion_headers.py (w17_chatcomple_3345ab).

The live ``_chat_completion_capturing_headers`` is replayed with scripted
agent/client/credits-tracker fakes; the branch taken, whether the raw call was
created, how many capture callbacks ran, and the returned value are compared.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


_pkg("agent")
NAME = "w17_chatcomple_3345ab_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "chat_completion_headers.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    provider = r.get("provider", "")
    has_state = bool(r.get("has_state", False))
    uw = r.get("is_usage_window", False)
    has_raw = bool(r.get("has_raw_response", False))
    response_window = bool(r.get("response_is_window", False))
    plain_result = r.get("plain_result", "PLAIN")
    parse_result = r.get("parse_result", "PARSED")
    raises = r.get("capture_raises", [False, False, False, False])

    rec = {"raw": 0, "caps": 0}
    state = types.SimpleNamespace(is_usage_window=uw) if has_state else None
    agent = types.SimpleNamespace(provider=provider, _credits_state=state)

    def mk(i):
        def cap(raw):
            rec["caps"] += 1
            if i < len(raises) and raises[i]:
                raise RuntimeError("capture failed")
        return cap
    agent._capture_rate_limits = mk(0)
    agent._capture_credits = mk(1)
    agent._capture_nous_model_switch = mk(2)
    agent._check_openrouter_cache_status = mk(3)

    def plain_create(**kw):
        return plain_result

    def raw_create(**kw):
        rec["raw"] += 1
        return raw
    comps = types.SimpleNamespace(create=plain_create)
    if has_raw:
        comps.with_raw_response = types.SimpleNamespace(create=raw_create)
    client = types.SimpleNamespace(chat=types.SimpleNamespace(completions=comps))
    raw = types.SimpleNamespace(parse=lambda: parse_result)

    ct = types.ModuleType("agent.credits_tracker")
    ct.response_is_usage_window = lambda rr: response_window
    old = sys.modules.get("agent.credits_tracker")
    sys.modules["agent.credits_tracker"] = ct
    try:
        result = live._chat_completion_capturing_headers(agent, client, dict(r.get("api_kwargs", {})))
    finally:
        if old is None:
            sys.modules.pop("agent.credits_tracker", None)
        else:
            sys.modules["agent.credits_tracker"] = old

    if rec["raw"] == 0:
        path = "plain"
    elif rec["caps"] == 0:
        path = "parse_only"
    else:
        path = "captured"
    return {"path": path, "raw_created": bool(rec["raw"]),
            "captures": int(rec["caps"]), "result": result}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w17_chatcomple_3345ab: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
