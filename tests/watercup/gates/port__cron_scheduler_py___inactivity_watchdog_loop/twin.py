"""twin for cron/scheduler.py:_inactivity_watchdog_loop (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_inactivity_watchdog_loop"
    C_FN = "_inactivity_watchdog_loop"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
