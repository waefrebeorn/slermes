#!/usr/bin/env python3
"""Differential oracle for live gateway/session_cli_turn.py (w10_sessioncli_1d2f43)."""
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
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AuthorityRefusal(Exception):
    pass


stub("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
NAME = "scli_1d2f43_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/gateway/session_cli_turn.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    try:
        if f == "submit_cli_turn_fields":
            return live.submit_cli_turn_fields(r.get("params") or {})
        if f == "take_cli_result_fields":
            return live.take_cli_result_fields(r.get("result") or {})
        if f == "admit_cli_turn":
            return live.admit_cli_turn(r.get("params") or {})
    except AuthorityRefusal as exc:
        return {"__raise__": "AuthorityRefusal", "msg": str(exc)}
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
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s: %r" % (r.get("func"), exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_sessioncli_1d2f43: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
