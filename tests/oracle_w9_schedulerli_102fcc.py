#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_liveness.py (gate w9_schedulerli_102fcc)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = QUARRY + "/cron/scheduler_liveness.py"
NAME = "scheduler_liveness_live_w9_schedulerli"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


class FakeStop:
    def __init__(self, n):
        self.n = n
        self.k = 0

    def wait(self, _poll):
        if self.k < self.n:
            self.k += 1
            return False
        return True


class IdentityMeter:
    def measure(self, idle_s):
        return idle_s


def compute(r):
    func = r.get("func")
    if func == "watchdog":
        samples = list(r.get("idles") or [])
        state = {"future_calls": 0}
        future_at = int(r.get("future_at", "-1"))

        def get_idle():
            return samples.pop(0) if samples else 0

        def future_done():
            i = state["future_calls"]
            state["future_calls"] += 1
            return i == future_at

        return bool(live._inactivity_watchdog_loop(
            get_idle_seconds=get_idle,
            limit_s=float(r.get("limit", 0.0)),
            poll_s=0.0,
            stop=FakeStop(int(r.get("stop_after", "0"))),
            future_done=future_done,
            meter=IdentityMeter(),
        ))
    if func == "stamper":
        class FakeTime:
            def __init__(self, seq):
                self.seq = list(seq)

            def monotonic(self):
                return self.seq.pop(0) if len(self.seq) > 1 else self.seq[0]

            def time(self):
                return 0.0

        original_time = live.time
        original_touch = live.touch_execution_progress
        original_owner = live.owner_progress
        touched = []
        try:
            live.time = FakeTime([0.0, r.get("elapsed", 0.0)])
            live.touch_execution_progress = lambda eid: touched.append(eid)
            live.owner_progress = {}
            st = live.ExecutionProgressStamper(
                r.get("execution_id", ""), "job",
                idle_seconds=lambda: r.get("idle", 0.0),
                every_seconds=r.get("every", 0.0),
            )
            st.tick()
            return bool(touched)
        finally:
            live.time = original_time
            live.touch_execution_progress = original_touch
            live.owner_progress = original_owner
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_schedulerli_102fcc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
