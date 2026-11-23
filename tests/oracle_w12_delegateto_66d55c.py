#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_tasks.py (gate w12_delegateto_66d55c)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_delegateto_66d55c"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/delegate_tool_tasks.py"
_spec = importlib.util.spec_from_file_location("delegate_tool_tasks_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegate_tool_tasks_live"] = live
_spec.loader.exec_module(live)

_FUNCS = ("normalize_task_images", "coerce_task_images", "validate_batch_tasks")


def live_out(rec):
    func = rec.get("func")
    if func == "normalize_task_images":
        return list(live._normalize_task_images(rec.get("task") or {}, int(rec.get("i", 0))))
    if func == "coerce_task_images":
        return list(live._coerce_task_images(rec.get("task_list") or [], rec.get("images")))
    if func == "validate_batch_tasks":
        return live._validate_batch_tasks(rec.get("task_list") or [])
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
