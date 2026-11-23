#!/usr/bin/env python3
"""Differential oracle for tools/skill_manager_batch.py (gate w12_skillmanag_069e36)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_skillmanag_069e36"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/skill_manager_batch.py"
_spec = importlib.util.spec_from_file_location("skill_manager_batch_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skill_manager_batch_live"] = live
_spec.loader.exec_module(live)

_FUNCS = ("misplaced_text_hint", "op_shape_error")


def live_out(rec):
    func = rec.get("func")
    action = rec.get("action") or ""
    args = rec.get("args") or {}
    if func == "misplaced_text_hint":
        return live._misplaced_text_hint(action, args)
    if func == "op_shape_error":
        return live._op_shape_error(action, args)
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
        if rec.get("func") not in _FUNCS:
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
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
