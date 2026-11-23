#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_tuning.py (gate w14_delegateto_e37eb6)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_delegateto_e37eb6"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/tools/delegate_tool_tuning.py"
_spec = importlib.util.spec_from_file_location("delegate_tool_tuning_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegate_tool_tuning_live"] = live
_spec.loader.exec_module(live)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") != "without_tier":
            continue
        cases += 1
        py = live.without_service_tier(rec.get("overrides"))
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
