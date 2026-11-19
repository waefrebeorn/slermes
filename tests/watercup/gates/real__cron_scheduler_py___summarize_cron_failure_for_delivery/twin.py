"""twin for cron/scheduler.py:_summarize_cron_failure_for_delivery (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_summarize_cron_failure_for_delivery"
    C_FN = "_summarize_cron_failure_for_delivery"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
