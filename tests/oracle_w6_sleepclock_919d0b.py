#!/usr/bin/env python3
"""Differential oracle for agent/sleep_clock.py (noc_w6_sleepclock_919d0b.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/agent/sleep_clock.py"
sys.path.insert(0, QUARRY)

spec = importlib.util.spec_from_file_location("sleep_clock_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sleep_clock_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", round(float(x), 9))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _slept(prev_wall, wall, interval):
    """Drive the LIVE SleepWatch.slept() with an injected wall clock."""
    sw = object.__new__(live.SleepWatch)
    sw._interval = interval
    sw._wall = prev_wall
    stub = types.ModuleType("sleep_clock_time_stub")
    stub.time = lambda: wall
    saved = live.time
    live.time = stub
    try:
        return sw.slept()
    finally:
        live.time = saved


def compute(r):
    f = r["func"]
    if f == "threshold":
        return live._SLEEP_THRESHOLD_S
    if f == "slept":
        return _slept(r["prev_wall"], r["wall"], r["interval"])
    if f == "sequence":
        prev = r["initial_wall"]
        vals = []
        for wall in r["walls"]:
            vals.append(_slept(prev, wall, r["interval"]))
            prev = wall
        return vals
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
        print(f"ORACLE sleepclock: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    if _canon(py) != _canon(r.get("out")):
        mism += 1
        print(f"MISMATCH sleepclock {r.get('func')} case={r.get('case')} "
              f"py={py!r} c={r.get('out')!r}", file=sys.stderr)
print(f"ORACLE sleepclock: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
