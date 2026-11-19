"""twin for cron/scheduler.py:_reclaim_fds_best_effort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_reclaim_fds_best_effort"
    C_FN = "_reclaim_fds_best_effort"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
