#!/usr/bin/env python3
"""Differential oracle for tools/connectors/contract.py (w20_contract_80eced).

The live module is imported straight out of the quarry (it has no import-time
dependencies beyond the stdlib), then every harness line is replayed against
its enums and tables.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_contract_80eced_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "tools", "connectors", "contract.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def transitions(kind):
    out = {}
    for (k, frm), edges in live.TRANSITIONS.items():
        if k == kind:
            out[frm.value] = {to.value: actor.value for to, actor in edges.items()}
    return out or None


def expected(r):
    f = r.get("func")
    if f == "resolves":
        return bool(live.resolves(r.get("kind"), r.get("state")))
    if f == "allowed":
        return live.allowed(r.get("kind"), r.get("current"), r.get("to"))
    if f == "transitions":
        return transitions(r.get("kind"))
    if f == "kinds":
        return list(live.KINDS)
    if f == "resolved_states":
        return sorted(m.value for m in live.RESOLVED_STATES)
    if f == "enum_values":
        enum = {"TargetState": live.TargetState, "Actor": live.Actor,
                "SettleReason": live.SettleReason}.get(r.get("enum"))
        return [m.value for m in enum] if enum is not None else None
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got), file=sys.stderr)
print("ORACLE w20_contract_80eced: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
