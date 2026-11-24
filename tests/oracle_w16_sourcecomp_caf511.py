#!/usr/bin/env python3
"""Differential oracle for hermes_cli/source_completion.py (w16_sourcecomp_caf511).

_acquire_waiting is replayed against the LIVE function with an injected
deterministic clock (the module-level `time` binding) and a scripted lock; the
captured notices and sleeps become part of the expected output.
"""
import contextlib
import importlib.util
import io
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/source_completion.py"
spec = importlib.util.spec_from_file_location("w16_sourcecomp_caf511_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w16_sourcecomp_caf511_live"] = live
spec.loader.exec_module(live)


class FakeTime:
    def __init__(self, clocks):
        self.clocks = list(clocks) or [0.0]
        self.sleeps = []

    def monotonic(self):
        if len(self.clocks) > 1:
            return self.clocks.pop(0)
        return self.clocks[0]

    def sleep(self, delay):
        self.sleeps.append(delay)


class FakeLock:
    def __init__(self, outcomes, pid, reason):
        self.outcomes = list(outcomes)
        self.holder = types.SimpleNamespace(pid=pid, reason=reason)

    def acquire(self):
        return self.outcomes.pop(0) if self.outcomes else True


def compute(r):
    func = r.get("func")
    if func == "prepared":
        return live._PREPARED
    if func == "lock_wait":
        return live.INSTALLER_LOCK_WAIT_SECONDS
    if func == "acquire":
        clock = FakeTime(r.get("clocks", [0.0]))
        old_time = live.time
        live.time = clock
        try:
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                verdict = live._acquire_waiting(
                    FakeLock(r.get("acquire", []), r.get("holder_pid", 0),
                             r.get("holder_reason")),
                    float(r.get("wait", 0.0)),
                )
        finally:
            live.time = old_time
        return {"ok": bool(verdict),
                "notices": buf.getvalue().splitlines(),
                "sleeps": clock.sleeps}
    return None


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
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w16_sourcecomp_caf511: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
