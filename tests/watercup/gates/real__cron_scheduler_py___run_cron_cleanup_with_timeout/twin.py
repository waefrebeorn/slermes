"""twin for cron/scheduler.py:_run_cron_cleanup_with_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_run_cron_cleanup_with_timeout"
    C_FN = "_run_cron_cleanup_with_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
