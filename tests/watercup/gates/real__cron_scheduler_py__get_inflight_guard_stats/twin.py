"""twin for cron/scheduler.py:get_inflight_guard_stats (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "get_inflight_guard_stats"
    C_FN = "get_inflight_guard_stats"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
