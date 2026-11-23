#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/scheduler_tick.py (schedtick gate)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)

# Bootstrap parent packages before loading the live module by path: the real
# cron package imports ruamel (absent in some interpreters), so supply the two
# leaf dependencies scheduler_tick actually uses at import time.
cron_pkg = types.ModuleType("cron")
cron_pkg.__path__ = []
sh = types.ModuleType("cron.store_health")
sh.UNWRITABLE_ERRNOS = {13, 28, 30}
sh.note_unwritable = lambda *a, **k: None
const = types.ModuleType("cron.constants")
const.is_recurring = lambda j: (j.get("schedule") or {}).get("kind") in {"cron", "interval"}
sys.modules["cron"] = cron_pkg
sys.modules["cron.store_health"] = sh
sys.modules["cron.constants"] = const

SRC = PYROOT + "/cron/scheduler_tick.py"
spec = importlib.util.spec_from_file_location("schedtick_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["schedtick_live"] = live
spec.loader.exec_module(live)


class Sched:
    def __init__(self, fail):
        self.fail = fail

    def advance_next_runs(self, ids):
        if self.fail:
            raise OSError("simulated unwritable store")


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "advance":
        kept = live._advance_or_drop_recurring(Sched(bool(r.get("raise_error"))),
                                               r.get("due_jobs", []))
        py = [j["id"] for j in kept]
    elif func == "yield":
        captured = []
        orig_mono, orig_err = live.time.monotonic, live.logger.error
        live.time.monotonic = lambda: r.get("now", 0.0)
        live.logger.error = lambda *a, **k: captured.append(a)
        try:
            live._log_tick_yield_once(r.get("reason", ""))
        finally:
            live.time.monotonic = orig_mono
            live.logger.error = orig_err
        py = {"logged": 1 if captured else 0}
    elif func == "const":
        py = live._YIELD_LOG_INTERVAL_SECONDS
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE schedtick: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
