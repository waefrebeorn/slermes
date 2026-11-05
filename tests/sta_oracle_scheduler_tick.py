#!/usr/bin/env python3
"""Oracle for cron/scheduler_tick.py _advance_or_drop_recurring and
cron/constants.py is_recurring. Both ast-extracted and run live; the store
advance is stubbed to either succeed or raise OSError.
"""
import ast
import json
import sys
import types

CON = "C:/Users/eman5/hermes-agent/cron/constants.py"
TICK = "C:/Users/eman5/hermes-agent/cron/scheduler_tick.py"

NS = {}


def _extract(path, names):
    code = open(path, encoding="utf-8").read()
    tree = ast.parse(code)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in names:
            seg = ast.get_source_segment(code, n)
            exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                    type_ignores=[]), "x", "exec"), NS)


_extract(CON, {"is_recurring"})
_extract(TICK, {"_advance_or_drop_recurring"})

# seed the scheduler_tick body's globals
NS["is_recurring"] = NS["is_recurring"]
NS["store_health"] = types.SimpleNamespace(
    note_unwritable=lambda *a, **k: None)
recur_ = NS["is_recurring"]
aod_ = NS["_advance_or_drop_recurring"]


class _Sched:
    def __init__(self, advance_ok):
        self.adv = advance_ok

    def advance_next_runs(self, ids):
        if not self.adv:
            raise OSError("read-only")
        return ids


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE scheduler_tick: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "recur":
            want = 1 if recur_(c["job"]) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH recur {c['case']}: C={c['out']} PY={want} "
                      f"job={c['job']!r}")
        elif fam == "aod":
            want = aod_(_Sched(bool(c["advance_ok"])), list(c["due"]))
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH aod {c['case']}: C={c['out']} PY={want}")
    print(f"ORACLE scheduler_tick: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
