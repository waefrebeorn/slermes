#!/usr/bin/env python3
"""Differential oracle for agent/redact_headers.py (gate w7_redactheade_29d7db).

Compares the two pure credential predicates against the live quarry module.
"""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/redact_headers.py"
spec = importlib.util.spec_from_file_location("w7_redactheade_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "cookie_credential":
        return live.cookie_value_is_credential(r.get("value", ""), qualified=bool(r.get("qualified", False)))
    if func == "json_header_credential":
        return live.json_header_value_is_credential(r.get("key", ""), r.get("value", ""))
    return None


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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_redactheade_29d7db: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
