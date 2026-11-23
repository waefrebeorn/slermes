#!/usr/bin/env python3
"""Differential oracle for live gateway/session_detached.py (w10_sessiondet_9727db)."""
import importlib.util
import json
import sys
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "sdet_9727db_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/gateway/session_detached.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "detached_key":
        return live.detached_key(r.get("route", ""), r.get("session_id", ""))
    if f == "detached_session_id":
        return live.detached_session_id(r.get("key", ""))
    if f == "own_detached_key":
        return live.own_detached_key(r.get("key", ""), r.get("session_id", ""))
    if f == "detached_route_of":
        src = r.get("source") or {}
        return live.detached_route_of(NS(**src) if isinstance(src, dict) else NS())
    if f == "running_key":
        runner = NS(_running_agents={k: True for k in (r.get("running") or [])})
        return live.running_key(runner, r.get("route", ""), r.get("session_id", ""))
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
print("ORACLE %s: %d cases, %d mismatches" % (G := "w10_sessiondet_9727db", cases, mism))
sys.exit(1 if mism else 0)
