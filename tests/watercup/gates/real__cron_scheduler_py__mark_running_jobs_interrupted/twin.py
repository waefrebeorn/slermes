"""twin for cron/scheduler.py:mark_running_jobs_interrupted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "mark_running_jobs_interrupted"
    C_FN = "mark_running_jobs_interrupted"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
