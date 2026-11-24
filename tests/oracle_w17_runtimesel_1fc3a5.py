#!/usr/bin/env python3
"""Differential oracle for agent/runtime_self_protection.py (w17_runtimesel_1fc3a5).

Replays the three pure helpers against the LIVE module: uv-pinning spec
matching, path-overlap (os.sep on this host) and command-prefix stripping.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w17_runtimesel_1fc3a5_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "runtime_self_protection.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "version_spec":
        return live._version_spec_matches(r.get("spec", ""), r.get("version", ""),
                                          r.get("uv_root_name", ""))
    if func == "overlaps":
        return live._overlaps(r.get("a", ""), r.get("b", ""))
    if func == "strip_prefixes":
        return live._strip_prefixes(r.get("words", []))
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w17_runtimesel_1fc3a5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
