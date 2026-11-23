#!/usr/bin/env python3
"""Differential oracle for gateway/session_a2a.py pure helpers (w7_sessiona2a_da57ab)."""
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


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


stub("persistence.core.errors", AuthorityRefusal=type("AuthorityRefusal", (Exception,), {}))
stub("gateway.session_contract", SessionRef=object, Submission=object)
stub("gateway.session_persistence", authority_persistence=lambda a: a)

LIVE_NAME = "w7_sessiona2a_da57ab_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/session_a2a.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    policy = types.SimpleNamespace(
        source=r.get("source"), platform=r.get("platform"),
        request_json=r.get("request_json"))
    if f == "is_forward_policy":
        return live.is_forward_policy(policy)
    if f == "forward_author":
        return live.forward_author(policy)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    py = compute(r)
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH sessiona2a %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_sessiona2a_da57ab: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
