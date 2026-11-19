"""twin for cron/scheduler.py:_cron_cleanup_timeout_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_cron_cleanup_timeout_seconds"
    C_FN = "_cron_cleanup_timeout_seconds"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
