#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_client.py
(gate w8_sessionclie_c302cf).

client_problem is compared directly. policy_client is driven with a policy
stand-in carrying request_json, exactly as the C seam describes.
"""
import importlib.util
import json
import sys
from types import SimpleNamespace

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("session_client_live", Q + "/gateway/session_client.py")
live = importlib.util.module_from_spec(spec)
sys.modules["session_client_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "client_problem":
            py = live.client_problem(r.get("value"))
        elif func == "policy_client":
            p = r.get("policy")
            py = live.policy_client(None if p is None else SimpleNamespace(request_json=p))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w8_sessionclie_c302cf: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
