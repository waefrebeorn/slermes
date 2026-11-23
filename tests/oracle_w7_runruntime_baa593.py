#!/usr/bin/env python3
"""Differential oracle for gateway/run_runtime.py parked-profile family (w7_runruntime_baa593)."""
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


# park_profile/unpark_profile publish through gateway.status.write_runtime_status.
stub("gateway.status", write_runtime_status=lambda **kw: None)

LIVE_NAME = "w7_runruntime_baa593_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/run_runtime.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    runner = types.SimpleNamespace(session_runtime_descriptor=r.get("descriptor"))
    if f == "profile_parked":
        return live.profile_parked(runner, r.get("name", ""))
    if f == "parked_profile_map":
        return live.parked_profile_map(runner)
    if f == "park_profile":
        live.park_profile(runner, r.get("name", ""), r.get("reason"))
        return runner.session_runtime_descriptor["parked_profiles"]
    if f == "unpark_profile":
        live.unpark_profile(runner, r.get("name", ""))
        return runner.session_runtime_descriptor["parked_profiles"]
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH runruntime %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_runruntime_baa593: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
