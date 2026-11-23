#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/worktree_sweep.py (gate w12_worktreesw_d88fbf)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = QUARRY + "/hermes_cli/worktree_sweep.py"
spec = importlib.util.spec_from_file_location("worktree_sweep_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["worktree_sweep_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "inside":
        return live._inside(r.get("target", ""), r.get("roots", []))
    if func == "classic_mode":
        return bool(live.classic_mode({
            "archive_dirty": bool(r.get("archive_dirty", False)),
            "max_total_gb": r.get("max_total_gb", 0),
        }))
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
print(f"ORACLE w12_worktreesw_d88fbf: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
