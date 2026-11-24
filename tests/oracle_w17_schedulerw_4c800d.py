#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_worker_failure.py (gate w17_schedulerw_4c800d).

cron.scheduler is injected as a deterministic collaborator: get_execution returns
the fixture's status, fire_claim_fence captures expected_owner and yields the
fixture's owns_claim, and the deeper log/delivery/mark hooks are no-ops. The LIVE
functions run for every case; the boolean result and captured fence owner are
compared against the C port's output object.
"""
import importlib.util
import json
import sys
import types
from contextlib import contextmanager

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

sched = types.ModuleType("cron.scheduler")
EXEC = {"status": "unknown"}
FO = {"v": None}
OWNS = {"v": False}
sched.get_execution = lambda eid: {"status": EXEC["status"], "error": "worker died"}
@contextmanager
def _fence(jid, expected_owner=None):
    FO["v"] = expected_owner
    yield OWNS["v"]
sched.fire_claim_fence = _fence
sched.logger = types.SimpleNamespace(error=lambda *a, **k: None)
sched._install_fire_secret_scope = lambda: ()
sched._reset_fire_secret_scope = lambda t: None
sched._deliver_crash_failure = lambda j, e, **k: (None, None)
sched.mark_job_run = lambda *a, **k: None
sys.modules["cron.scheduler"] = sched

ur = types.ModuleType("cron.unreachable_retry")
ur.is_retry_run = lambda j: False
sys.modules["cron.unreachable_retry"] = ur

SRC = QUARRY + "/cron/scheduler_worker_failure.py"
spec = importlib.util.spec_from_file_location("w17_schedulerw_live", SRC)
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
    try:
        if func == "reason":
            py = live.external_worker_exited_reason(r.get("rc"))
        elif func == "record":
            EXEC["status"] = r.get("exec_status", "unknown")
            OWNS["v"] = bool(r.get("owns_claim"))
            FO["v"] = None
            job = json.loads(json.dumps(r.get("job") or {}))
            res = live.record_unknown_worker_outcome(job)
            py = {"result": res, "fence_owner": FO["v"]}
        else:
            continue
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w17_schedulerw_4c800d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
