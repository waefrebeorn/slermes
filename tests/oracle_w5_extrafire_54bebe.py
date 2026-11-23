#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/extra_fire.py (gate w5_extrafire_54bebe)."""
import copy
import importlib.util
import json
import sys
import types
from datetime import datetime, timezone

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("cron", QUARRY + "/cron")


def _parse_aware(value):
    if not isinstance(value, str):
        return None
    try:
        d = datetime.fromisoformat(value)
        if d.tzinfo is None:
            d = d.replace(tzinfo=timezone.utc)
        return d
    except Exception:
        return None


jobs = types.ModuleType("cron.jobs")
jobs._parse_aware = _parse_aware
sys.modules["cron.jobs"] = jobs

spec = importlib.util.spec_from_file_location("cron.extra_fire", QUARRY + "/cron/extra_fire.py")
live = importlib.util.module_from_spec(spec)
sys.modules["cron.extra_fire"] = live
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
    job = r.get("job") or {}
    if func == "queued":
        py = live.queued(job)
    elif func == "restore":
        py = live.restore_target(job)
    elif func == "is_extra":
        py = live.is_extra(job)
    elif func == "held":
        py = live.held_markers(job)
    elif func == "pending":
        py = live.pending_slot(job, datetime.fromisoformat(r["now"]), bool(r.get("manual_fire")))
    elif func == "stamp":
        j = copy.deepcopy(job)
        live.stamp_claim(j, r.get("slot", ""))
        py = j
    elif func == "reschedule":
        j = copy.deepcopy(job)
        live.reschedule(j, r.get("at", ""))
        py = j
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w5_extrafire_54bebe: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
