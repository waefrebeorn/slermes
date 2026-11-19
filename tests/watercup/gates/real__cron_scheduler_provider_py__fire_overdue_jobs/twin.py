"""twin for cron/scheduler_provider.py:fire_overdue_jobs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler_provider.py"
    PY_FN = "fire_overdue_jobs"
    C_FN = "fire_overdue_jobs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
