#!/usr/bin/env python3
"""Differential oracle for cron/job_args.py pure helpers (w5_jobargs_45f9c5).

Import-time dependencies (cron.jobs.effective_job_state, hermes_time) are
stubbed; every tested helper is pure and called with the fixture values.
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


_stub("cron")
_stub("cron.jobs", effective_job_state=lambda job: "enabled")
_stub("hermes_time", now=lambda: None)
sys.modules["cron"].jobs = sys.modules["cron.jobs"]

LIVE_NAME = "w5_jobargs_45f9c5_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/cron/job_args.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "split_monitor_arg":
        return list(live._split_monitor_arg(r.get("monitor"), r.get("monitor_script"),
                                            r.get("monitor_url")))
    if f == "repeat_display":
        return live._repeat_display(r.get("job") or {})
    if f == "clean_str_list":
        return live._clean_str_list(r.get("items"))
    if f == "canonical_skills":
        return live._canonical_skills(r.get("skill"), r.get("skills"))
    if f == "normalize_optional":
        return live._normalize_optional_job_value(r.get("value"),
                    strip_trailing_slash=bool(r.get("strip_trailing_slash", False)))
    if f == "normalize_deliver":
        return live._normalize_deliver_param(r.get("value"))
    if f == "apply_continuity":
        return live._apply_continuity(r.get("context_from"), bool(r.get("continuity", False)))
    if f == "base_url_refused":
        return live._base_url_refused(r.get("bu", ""), r.get("prov", ""), r.get("why", ""))
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
        print("MISMATCH jobargs %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_jobargs_45f9c5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
