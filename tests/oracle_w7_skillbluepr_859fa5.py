#!/usr/bin/env python3
"""Differential oracle for cron/skill_blueprints.py (gate w7_skillbluepr_859fa5)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

LIVE_NAME = "w7_skillbluepr_859fa5_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/cron/skill_blueprints.py")
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
    if r.get("func") != "blueprint_to_job_spec":
        continue
    spec_obj = live.BlueprintSpec(
        skill_name=r.get("skill_name", ""),
        schedule=r.get("schedule", ""),
        deliver=r.get("deliver", "origin"),
        prompt=r.get("prompt"),
        no_agent=bool(r.get("no_agent", False)),
        model=r.get("model"),
        provider=r.get("provider"),
        enabled_toolsets=r.get("enabled_toolsets"),
        raw={},
    )
    name = r.get("name")
    py = live.blueprint_to_job_spec(spec_obj, name=name)

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH blueprint_to_job_spec args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_skillbluepr_859fa5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
