#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_runtime_progress.py (w20_gatewayrun_1cb73c).

The live ``StartRunway`` is driven with its two I/O seams injected: a scripted
``time.monotonic`` (the module's ``time`` is replaced for the duration) and
scripted ``_starting_owner_pid`` / ``_cpu_seconds``.  Every deadline the real
class returns is compared; the construction-time clock reading is the first
step's ``now``.
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

sp = importlib.util.spec_from_file_location(
    "w20_gatewayrun_1cb73c_live",
    os.path.join(Q, "hermes_cli", "gateway_runtime_progress.py"))
live = importlib.util.module_from_spec(sp)
sys.modules["w20_gatewayrun_1cb73c_live"] = live
sp.loader.exec_module(live)

REAL_TIME = live.time
CLOCK = [0.0]
SEAM = {"pid": None, "cpu": None}


def replay(r):
    steps = r.get("steps")
    if not isinstance(steps, list):
        steps = []
    CLOCK[0] = steps[0].get("now", 0.0) if steps else 0.0
    live.time = types.SimpleNamespace(monotonic=lambda: CLOCK[0])
    live._starting_owner_pid = lambda homes, child: SEAM["pid"]
    live._cpu_seconds = lambda pid: SEAM["cpu"]
    try:
        runway = live.StartRunway(r.get("timeout"), r.get("default"))
    except Exception as exc:  # noqa: BLE001 - ValueError, or TypeError from math.isfinite
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
    out = []
    for st in steps:
        CLOCK[0] = st.get("now", 0.0)
        SEAM["pid"] = st.get("owner_pid")
        SEAM["cpu"] = st.get("cpu")
        child = st.get("child")
        child_obj = types.SimpleNamespace(pid=child["pid"]) if isinstance(child, dict) else None
        out.append(runway.extend(st.get("homes") or [], child_obj))
    return {"status": "ok", "deadlines": out}


def expected(r):
    f = r.get("func")
    if f == "constants":
        return {
            "max_adaptive_timeout": live.MAX_ADAPTIVE_TIMEOUT,
            "progress_window_s": live.PROGRESS_WINDOW_S,
            "min_cpu_per_window_s": live._MIN_CPU_PER_WINDOW_S,
            "lead_s": live._LEAD_S,
            "sample_every_s": live._SAMPLE_EVERY_S,
        }
    if f == "run":
        return replay(r)
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
try:
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
            print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            mism += 1
            print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
                  file=sys.stderr)
finally:
    live.time = REAL_TIME
print("ORACLE w20_gatewayrun_1cb73c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
