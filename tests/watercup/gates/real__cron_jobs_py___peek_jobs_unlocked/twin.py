"""twin for cron/jobs.py:_peek_jobs_unlocked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_peek_jobs_unlocked"
    C_FN = "_peek_jobs_unlocked"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
