#!/usr/bin/env python3
"""Differential oracle for tools/skill_lock.py (gate w6_skilllock_842861)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/skill_lock.py"
spec = importlib.util.spec_from_file_location("w6_skilllock_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_skilllock_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "skill_refusal":
        return live.skill_refusal(r.get("name", ""))
    if f == "unquoted_newlines_to_semicolons":
        return live._unquoted_newlines_to_semicolons(r.get("command", ""))
    if f == "segments":
        return [list(x) for x in live._segments(r.get("tokens", []))]
    if f == "operands":
        return live._operands(r.get("args", []))
    if f == "target_directory":
        d, rest = live._target_directory(r.get("args", []))
        return [d, rest]
    if f == "find_exec_commands":
        return [list(x) for x in live._find_exec_commands(r.get("args", []))]
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_skilllock_842861: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
