#!/usr/bin/env python3
"""Differential oracle for gate w10_skillstool_52fa6b (tools/skills_tool_dedup.py).
Replays the harness output in order so the module-level dedup cache stays in
lockstep with the C process."""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/skills_tool_dedup.py"
spec = importlib.util.spec_from_file_location("w10dedup_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "dedup":
        return live.dedup_skill_view(r.get("task"), r.get("name", ""), r.get("file_path", ""),
                                     r.get("payload") or {}, r.get("result", ""))
    if f == "reset":
        live.reset_skill_view_dedup(r.get("task"))
        return None
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
    except BaseException as e:
        py = {"__raise__": type(e).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w10_skillstool_52fa6b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
