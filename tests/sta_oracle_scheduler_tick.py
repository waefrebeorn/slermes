#!/usr/bin/env python3
"""Oracle for cron/scheduler_tick.py pure core:
- is_recurring (cron/constants.py) — ast-extracted, run live on job JSON.
- _advance_or_drop_recurring (scheduler_tick.py) — ast-extracted, store	stubs.
- _log_tick_yield_once decision+message — ast-extracted, driven in a seeded
  namespace (capture logger, mocked time.monotonic, preset _last_yield_log).

The harness emits {"family":..., ...} lines; family in
{recur, aod, gate, msg}; each with "out" = the C result.
"""
import ast
import json
import sys
import time
import types

sys.stdin.reconfigure(encoding="utf-8")
CON = "C:/Users/eman5/hermes-agent/cron/constants.py"
TICK = "C:/Users/eman5/hermes-agent/cron/scheduler_tick.py"


def _extract(path, names):
    code = open(path, encoding="utf-8").read()
    tree = ast.parse(code)
    out = {}
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in names:
            seg = ast.get_source_segment(code, n)
            node = ast.parse(seg).body[0]
            exec(compile(ast.Module(body=[node], type_ignores=[]), "x", "exec"), out)
    return out


CON_NS = _extract(CON, {"is_recurring"})
recur_ = CON_NS["is_recurring"]
NS = _extract(TICK, {"_advance_or_drop_recurring", "_log_tick_yield_once"})
aod_ = NS["_advance_or_drop_recurring"]
lty_ = NS["_log_tick_yield_once"]
# seed the extracted _advance_or_drop_recurring's module globals (it references
# is_recurring and store_health.note_unwritable at call time).
NS["is_recurring"] = recur_
NS["store_health"] = types.SimpleNamespace(note_unwritable=lambda *a, **k: None)


class _Logger:
    def __init__(self):
        self.calls = []

    def error(self, msg, *args):
        self.calls.append((msg, args))


class _Standin:
    def __init__(self, advance_ok):
        self.adv = advance_ok

    def advance_next_runs(self, ids):
        if not self.adv:
            raise OSError("read-only")
        return ids


def _defnode(path, name):
    code = open(path, encoding="utf-8").read()
    tree = ast.parse(code)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name == name:
            seg = ast.get_source_segment(code, n)
            return ast.parse(seg).body[0]
    raise KeyError(name)


def run_tick_yield(reason, now, last_reason, last_at):
    logger = _Logger()
    clock = types.SimpleNamespace(monotonic=lambda: now)
    ns = {
        "logger": logger,
        "time": clock,
        "_YIELD_LOG_INTERVAL_SECONDS": 3600.0,
        "_last_yield_log": {"reason": last_reason, "at": last_at},
    }
    # exec the extracted DEF (not its code body) into a fresh namespace, then call
    # the defined name; it references logger/time/_last_yield_log as globals.
    defn = _defnode(TICK, "_log_tick_yield_once")
    exec(compile(ast.Module(body=[defn], type_ignores=[]), "<y>", "exec"), ns)
    ns["_log_tick_yield_once"](reason)
    fired = 1 if logger.calls else 0
    msg = logger.calls[0][0] % logger.calls[0][1] if logger.calls else None
    return fired, msg


passes = 0
fails = []
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    fam = c.get("family")
    if fam == "recur":
        want = 1 if recur_(c["job"]) else 0
        ok = want == c["out"]
        total += 1
        if ok: passes += 1
        else: fails.append(("recur", c.get("case"), want, c["out"]))
    elif fam == "aod":
        want = aod_(_Standin(bool(c["advance_ok"])), list(c["due"]))
        ok = want == c["out"]
        total += 1
        if ok: passes += 1
        else: fails.append(("aod", c.get("case"), want, c["out"]))
    elif fam == "gate":
        lr = c["lr"]
        la = c["la"]
        r = c["r"]
        now = c["now"]
        # independent predicate for the decision; live 'fired' must equal it
        pred = 1 if (lr != r or (now - (la if la is not None else 0.0)) >= 3600.0) else 0
        total += 1

        # drive the LIVE def to observe whether it fires and what message it logs
        fired, _msg = run_tick_yield(r, now, lr, la)
        if fired == pred and fired == c["out"]:
            passes += 1
        else:
            fails.append(("gate", (lr, la, r, now), pred, c["out"], fired))
    elif fam == "msg":
        r = c["r"] or ""
        expected = (
            "Cron tick yielded: this process is running stale code (%s) and a "
            "fresher gateway owns the runtime lock \u2014 jobs will fire from that "
            "process. Restart this one to reclaim its ticks." % r)
        ok = c["out"] == expected
        total += 1
        if ok: passes += 1
        else: fails.append(("msg", r, expected, c["out"]))

print("pass/total: %d/%d" % (passes, total))
for f in fails[:10]:
    print("FAIL", f[0], f[1], "want=", f[2], "c=", f[3])
if fails:
    print("ORACLE scheduler_tick: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE scheduler_tick: %d cases, 0 mismatches" % total)
