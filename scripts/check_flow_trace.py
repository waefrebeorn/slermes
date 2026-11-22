#!/usr/bin/env python3
"""Validate docs/dev/flow_trace.jsonl entries against the current tree."""
import json
import os
import sys

p = "docs/dev/flow_trace.jsonl"
bad = []
n = 0
for line in open(p, encoding="utf-8"):
    line = line.strip()
    if not line:
        continue
    n += 1
    try:
        r = json.loads(line)
    except Exception as exc:
        bad.append(f"line {n}: bad JSON: {exc}")
        continue
    for key in ("id", "date", "file", "caller", "callee", "symptom", "root_cause", "fix", "evidence", "flow_trace"):
        if not r.get(key):
            bad.append(f"line {n}: missing {key}")
    if not os.path.isfile(r.get("file", "")):
        bad.append(f"line {n}: missing file {r.get('file')}")
    if "cases, 0 mismatches" not in r.get("evidence", "") and r.get("evidence") != "check_port_wiring.py: wired":
        bad.append(f"line {n}: evidence is not a verified gate/wiring result")
print(f"flow trace entries: {n}")
if bad:
    for b in bad:
        print("BAD " + b)
    sys.exit(1)
print("VERDICT: every entry has a verified result and an existing C file")
