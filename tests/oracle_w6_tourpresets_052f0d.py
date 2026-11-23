#!/usr/bin/env python3
"""Differential oracle for tools/tour_presets.py (gate w6_tourpresets_052f0d)."""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = "C:/Users/eman5/hermes-agent/tools/tour_presets.py"
spec = importlib.util.spec_from_file_location("tourpresets_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tourpresets_live"] = live
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
        if func == "is_valid":
            try:
                live.TourPreset(r.get("name"))
                py = True
            except ValueError:
                py = False
        elif func == "coerce":
            try:
                py = live.TourPreset(r.get("name")).value
            except ValueError:
                py = None
        elif func == "values":
            py = [e.value for e in live.TourPreset]
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % ("w6_tourpresets_052f0d", cases, mism))
sys.exit(1 if mism or not cases else 0)
