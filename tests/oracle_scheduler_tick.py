#!/usr/bin/env python3
"""Oracle for cron/scheduler_tick.py pure cores + cron/constants.py:is_recurring.

Harness (t_port_scheduler_tick.c) emits per family:
  recur: {family:"recur", job:<json>, out:<0|1>}        -> is_recurring(job)
  aod:   {family:"aod", due:<json-array>, advance_ok:<0|1>, out:<json or "null">}
           -> _advance_or_drop_recurring(fake_sched, due) where fake_sched.advance_next_runs
              raises OSError when advance_ok==0
  gate:  {family:"gate", lr:<str|null>, la:float, r:<str|null>, now:float, out:bool}
           -> _log_tick_yield_once decision (should it log)
  msg:   {family:"msg", r:<str>, out:<str>}             -> the yield message
is_recurring & _advance_or_drop_recurring & _log_tick_yield_once are stateful
only through caller-passed state (_sched, _last_yield_log globals, time.monotonic,
store_health); the oracle supplies fakes, the same way the port takes them as args.
"""
import ast
import json
import types
import sys
import time as _time

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

CST = "C:/Users/eman5/hermes-agent/cron/constants.py"
SRC = "C:/Users/eman5/hermes-agent/cron/scheduler_tick.py"
_YIELD_LOG_INTERVAL_SECONDS = 3600.0
_YIELD_MSG = ("Cron tick yielded: this process is running stale code (%s) and a "
              "fresher gateway owns the runtime lock — jobs will fire from that "
              "process. Restart this one to reclaim its ticks.")


def main() -> int:
    cst = open(CST, encoding="utf-8").read()
    cns = {"Any": object}
    cbody = [n for n in ast.parse(cst).body
             if isinstance(n, ast.FunctionDef) and n.name == "is_recurring"]
    exec(compile(ast.Module(body=cbody, type_ignores=[]), CST, "exec"), cns)
    is_recurring = cns["is_recurring"]

    src = open(SRC, encoding="utf-8").read()
    ns = {
        "logger": types.SimpleNamespace(error=lambda *a, **k: None),
        "store_health": types.SimpleNamespace(note_unwritable=lambda *a, **k: None),
        "is_recurring": is_recurring,
        "_last_yield_log": {"reason": None, "at": 0.0},
        "time": _time,
        "Any": object,
        "_YIELD_LOG_INTERVAL_SECONDS": _YIELD_LOG_INTERVAL_SECONDS,
    }
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in
            ("_advance_or_drop_recurring", "_log_tick_yield_once")]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "recur":
            want = 1 if is_recurring(c.get("job")) else 0
            got = int(c.get("out"))
            if want != got:
                mism += 1
                print(f"MISMATCH recur case={c.get('case')} C={got} PY={want}")
        elif fam == "aod":
            class FakeSched:
                def __init__(self, ok):
                    self.ok = ok
                def advance_next_runs(self, ids):
                    if not self.ok:
                        raise OSError("store refused")
                    return True
            sched = FakeSched(int(c.get("advance_ok")) == 1)
            ns["_last_yield_log"] = {"reason": None, "at": 0.0}
            want = ns["_advance_or_drop_recurring"](sched, list(c.get("due")))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH aod case={c.get('case')} C={got!r} PY={want!r}")
        elif fam == "gate":
            # decision: log iff last_reason != reason or now - last_at >= interval
            lr = c.get("lr"); r = c.get("r")
            want = 1 if (lr != r or (float(c["now"]) - float(c["la"])) >= _YIELD_LOG_INTERVAL_SECONDS) else 0
            got = 1 if c.get("out") in ("true", True) else 0
            if want != got:
                mism += 1
                print(f"MISMATCH gate case={c.get('case')} C={got} PY={want}")
        elif fam == "msg":
            want = _YIELD_MSG % (c.get("r") or "")
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH msg case={c.get('case')} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE scheduler_tick: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE scheduler_tick: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
