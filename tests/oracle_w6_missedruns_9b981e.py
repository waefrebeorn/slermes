#!/usr/bin/env python3
"""Differential oracle for the C11 port of cron/missed_runs.py (w6_missedruns_9b981e).

Loads the LIVE module by path with a stubbed `cron` parent package; the two
lazy module-level lookups in job_catches_up are injected via sys.modules
registrations whose results come from the harness-echoed fixture keys.
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

state = {"grace": False, "cfg": False}

cron_pkg = types.ModuleType("cron")
cron_pkg.__path__ = []
sys.modules["cron"] = cron_pkg

jobs_time = types.ModuleType("cron.jobs_time")
jobs_time.grace_seconds_override = lambda g: (60 if state["grace"] else None)
sys.modules["cron.jobs_time"] = jobs_time

jobs_mod = types.ModuleType("cron.jobs")
jobs_mod._cron_config_number = lambda key, default, conv: state["cfg"]
sys.modules["cron.jobs"] = jobs_mod

SRC = Q + "/cron/missed_runs.py"
spec = importlib.util.spec_from_file_location("w6_missedruns_9b981e_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_missedruns_9b981e_live"] = live
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
    if r.get("func") != "catches_up":
        continue
    state["grace"] = bool(r.get("grace_override", False))
    state["cfg"] = bool(r.get("config_catch_up", False))
    try:
        py = bool(live.job_catches_up(r.get("job") or {}))
    except Exception as exc:
        print("LIVE ERROR case=%s: %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s C=%r PY=%r" % (r.get("case"), got, py), file=sys.stderr)

if cases == 0:
    print("ORACLE w6_missedruns_9b981e: 0 cases (link failure in disguise)")
    sys.exit(1)
print("ORACLE w6_missedruns_9b981e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
