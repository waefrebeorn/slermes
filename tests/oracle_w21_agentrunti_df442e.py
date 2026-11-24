#!/usr/bin/env python3
"""Differential oracle for agent/agent_runtime_helpers_placeholders.py
(gate w21_agentrunti_df442e).

The live module is imported as-is and is pure stdlib with no imports of its
own: the two module literals and `hidden_interrupt_placeholder_row()` are the
whole surface.  `matches` is the replay filter's retirement test, computed
against the LIVE constants.
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_agentrunti_df442e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("agent.agent_runtime_helpers_placeholders")
CURRENT = live._INTERRUPTED_PLACEHOLDER
LEGACY = live._LEGACY_INTERRUPTED_PLACEHOLDER


def expected(r):
    f = r.get("func")
    if f == "row":
        return live.hidden_interrupt_placeholder_row()
    if f == "placeholder":
        return CURRENT
    if f == "legacy":
        return LEGACY
    if f == "matches":
        return r.get("text") in (CURRENT, LEGACY)
    return "UNKNOWN_FUNC"


def main():
    cases = mism = 0
    wanted = {"row", "placeholder", "legacy", "matches"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        py = expected(r)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
