"""twin for cron/jobs.py:_recoverable_oneshot_run_at (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_recoverable_oneshot_run_at"
    C_FN = "_recoverable_oneshot_run_at"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
