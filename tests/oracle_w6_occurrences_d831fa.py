#!/usr/bin/env python3
"""Differential oracle for the C11 port of cron/occurrences.py (w6_occurrences_d831fa).

Loads the LIVE module by path with a stub `cron` parent package (the real one
pulls ruamel through utils). Compares scheduled_instant() on each echoed
fixture value against the harness output.
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

cron_pkg = types.ModuleType("cron")
cron_pkg.__path__ = []
sys.modules["cron"] = cron_pkg

SRC = Q + "/cron/occurrences.py"
spec = importlib.util.spec_from_file_location("w6_occurrences_d831fa_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_occurrences_d831fa_live"] = live
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
    if r.get("func") != "instant":
        continue
    try:
        py = live.scheduled_instant(r.get("value"))
    except Exception as exc:
        print("LIVE ERROR case=%s: %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s value=%r C=%r PY=%r" %
              (r.get("case"), r.get("value"), got, py), file=sys.stderr)

if cases == 0:
    print("ORACLE w6_occurrences_d831fa: 0 cases (link failure in disguise)")
    sys.exit(1)
print("ORACLE w6_occurrences_d831fa: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
