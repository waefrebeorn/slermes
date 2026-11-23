#!/usr/bin/env python3
"""Differential oracle for tools/bot_failure_reasons.py (gate w12_botfailure_9eb29a)."""
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
G = "w12_botfailure_9eb29a"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/bot_failure_reasons.py"
_spec = importlib.util.spec_from_file_location("bot_failure_reasons_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["bot_failure_reasons_live"] = live
_spec.loader.exec_module(live)

FUNCS = ("retry_action", "turn_failure_text", "classify", "result_retry")


def live_out(rec):
    func = rec.get("func")
    if func == "retry_action":
        return live.retry_action(rec.get("reason", ""))
    if func == "turn_failure_text":
        return live.turn_failure_text(rec.get("stdout"), rec.get("stderr"))
    if func == "classify":
        return live.classify_agent_error(rec.get("text", ""))
    if func == "result_retry":
        return live.result_retry_action(rec.get("result"))
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
