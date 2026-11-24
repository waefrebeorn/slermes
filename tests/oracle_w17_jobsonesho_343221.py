#!/usr/bin/env python3
"""Differential oracle for cron/jobs_oneshot_diagnostics.py (gate w17_jobsonesho_343221).

cron.jobs is injected as a deterministic collaborator: save_job_output captures
the diagnostic text, _hermes_now returns the fixture's fixed ISO timestamp, and
ONESHOT_GRACE_SECONDS comes from the fixture. The LIVE write_wedged / write_missed
bodies run for every case; the captured text is compared to the C port's output.
"""
import datetime
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

cron_pkg = types.ModuleType("cron")
cron_pkg.__path__ = []
sys.modules["cron"] = cron_pkg

captured = []
NOW = {"v": "2026-05-06T07:08:09.123456"}
GRACE = {"v": 4321}

jobs = types.ModuleType("cron.jobs")
def save_job_output(job_id, text, *a, **k):
    captured.append((job_id, text))
jobs.save_job_output = save_job_output
def _hermes_now():
    return datetime.datetime.fromisoformat(NOW["v"])
jobs._hermes_now = _hermes_now
jobs.ONESHOT_GRACE_SECONDS = GRACE["v"]
sys.modules["cron.jobs"] = jobs

SRC = QUARRY + "/cron/jobs_oneshot_diagnostics.py"
spec = importlib.util.spec_from_file_location("w17_jobsonesho_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
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
    if func not in ("wedged", "missed"):
        continue
    NOW["v"] = r.get("now_iso") or "2026-05-06T07:08:09.123456"
    GRACE["v"] = int(r.get("grace", "4321"))
    jobs.ONESHOT_GRACE_SECONDS = GRACE["v"]
    captured.clear()
    job = json.loads(json.dumps(r.get("job") or {}))
    try:
        if func == "wedged":
            live.write_wedged(job)
        else:
            live.write_missed(job, r.get("next_run", ""))
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    py = captured[0][1] if captured else None
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w17_jobsonesho_343221: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
