#!/usr/bin/env python3
"""Differential oracle for cron/delivery_outcome.py (gate w7_deliveryout_305ac3)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("cron")

LIVE_NAME = "w7_deliveryout_305ac3_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/cron/delivery_outcome.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func not in ("terminal", "settled_outcome", "settle_queued"):
        continue

    status = r.get("status")
    row = None
    if status is not None:
        row = {"status": status}
        if "error" in r and r["error"] is not None:
            row["error"] = r["error"]
    _stub("cron.delivery_queue", get_status=lambda eid: row)
    _stub("cron.executions", latest_execution=lambda jid: None,
          settle_delivery_outcome=lambda eid, outcome: None)
    _stub("cron.jobs", _with_job=lambda jid, apply, missing=False: False,
          save_jobs=lambda jobs: None)
    _stub("cron.jobs_error_text", sanitize_persisted_error=lambda e: e)

    if func == "terminal":
        t = live._terminal("exec-1")
        py = list(t) if t is not None else None
    elif func == "settled_outcome":
        py = live.settled_outcome("exec-1")
    else:
        jid = r.get("job_id")
        eid = r.get("execution_id")
        py = live.settle_queued_delivery(jid, eid)

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s args=%r py=%r c=%r" % (func, r, py, got), file=sys.stderr)

print("ORACLE w7_deliveryout_305ac3: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
