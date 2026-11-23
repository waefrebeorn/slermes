#!/usr/bin/env python3
"""Differential oracle for cron/job_definition.py (gate w6_jobdefiniti_2ab816).

merge_job_definition is the live function. Its store/zone/schedule helpers are
stubbed with deterministic seams that the C port mirrors:
  normalize_timezone = identity, prune_inherited_knobs = no-op,
  parse_schedule = identity, is_job_runnable = bool(job.get("schedule")),
  _clear_quota_hold = no-op, _apply_schedule_update = dict.update.
"""
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
_stub("cron.jobs",
      _apply_schedule_update=lambda d, u, name: d.update(u),
      _jobs_lock=lambda: None,
      is_job_runnable=lambda job: bool(job.get("schedule")),
      load_jobs=lambda: [],
      parse_schedule=lambda s: s,
      save_jobs=lambda jobs: None)
_stub("cron.jobs_time",
      normalize_timezone=lambda z: z,
      prune_inherited_knobs=lambda d: None)
_stub("cron.quota_hold", clear_state=lambda d: None)
_stub("hermes_time", now=lambda: None)

LIVE_NAME = "w6_jobdefiniti_2ab816_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/cron/job_definition.py")
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
    if r.get("func") != "merge":
        continue

    py = live.merge_job_definition(r.get("local") or {}, r.get("authored") or {})

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH merge args=%r\n  py=%r\n  c =%r" % (r, py, got), file=sys.stderr)

print("ORACLE w6_jobdefiniti_2ab816: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
