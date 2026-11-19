"""twin for cron/scheduler.py:_cron_job_origin_log_suffix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_cron_job_origin_log_suffix"
    C_FN = "_cron_job_origin_log_suffix"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
