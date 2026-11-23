#!/usr/bin/env python3
"""Differential oracle for tools/bot_mode_dm_outcome.py (gate w12_botmodedm_efcb4f)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_botmodedm_efcb4f"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/bot_mode_dm_outcome.py"
_spec = importlib.util.spec_from_file_location("bot_mode_dm_outcome_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["bot_mode_dm_outcome_live"] = live
_spec.loader.exec_module(live)

FUNCS = ("outcome_line", "combined_summary")


def live_out(rec):
    func = rec.get("func")
    if func == "outcome_line":
        return live.outcome_line(rec.get("to"), rec["entry"])
    if func == "combined_summary":
        return live.combined_summary(rec["entries"])
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r} input={rec!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
