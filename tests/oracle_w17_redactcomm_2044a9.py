#!/usr/bin/env python3
"""Differential oracle for agent/redact_command_segments.py (w17_redactcomm_2044a9)."""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w17_redactcomm_2044a9_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "redact_command_segments.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    return live._command_segments(r.get("command", ""))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w17_redactcomm_2044a9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
