#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_authority.py (gate w10_schedulera_538417)."""
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

SRC = QUARRY + "/cron/scheduler_authority.py"
NAME = "scheduler_authority_live_w10_schedulera"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "pause_identity":
        return live._pause_identity(r.get("paused"))
    if func == "replayable":
        existing = set(str(x) for x in (r.get("existing") or []))

        class FakeJournal:
            def __init__(self, job_id, eid):
                self.eid = eid

            def exists(self):
                return str(self.eid) in existing

        original = live.journal_path
        try:
            live.journal_path = FakeJournal
            return list(live.replayable_completions(
                r.get("job_id", ""), r.get("completions")))
        finally:
            live.journal_path = original
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
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w10_schedulera_538417: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
