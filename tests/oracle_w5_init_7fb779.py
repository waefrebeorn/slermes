#!/usr/bin/env python3
"""Differential oracle for cron/__init__.py package surface (w5_init_7fb779).

The worker bootstrap and the cron.jobs/cron.scheduler imports are stubbed with
identity sentinels so the re-export map can be checked against the live module.
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


def _stub(name, **attrs):
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


JOB_NAMES = ["create_job", "get_job", "list_jobs", "remove_job", "update_job",
             "pause_job", "resume_job", "trigger_job", "rearm_oneshot", "JOBS_FILE"]
job_sentinel = {name: object() for name in JOB_NAMES}
tick_sentinel = object()
boot_calls = []
_stub("cron")
_stub("cron.worker_bootstrap", worker_bootstrap=lambda: boot_calls.append(1))
_stub("cron.jobs", **job_sentinel)
_stub("cron.scheduler", tick=tick_sentinel)
for _sub in ("worker_bootstrap", "jobs", "scheduler"):
    setattr(sys.modules["cron"], _sub, sys.modules["cron." + _sub])

LIVE_NAME = "w5_init_7fb779_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/cron/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "all":
        return list(live.__all__)
    if f == "has":
        return r.get("name") in live.__all__
    if f == "exports":
        out = {}
        for name in live.__all__:
            value = getattr(live, name, None)
            if value is tick_sentinel:
                out[name] = "cron.scheduler"
            elif any(value is sentinel for sentinel in job_sentinel.values()):
                out[name] = "cron.jobs"
            else:
                out[name] = "UNKNOWN"
        return out
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
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH cron_init %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_init_7fb779: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
