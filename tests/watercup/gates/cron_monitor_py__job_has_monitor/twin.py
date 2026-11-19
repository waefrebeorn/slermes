"""twin for cron/monitor.py:job_has_monitor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/monitor.py"
    PY_FN = "job_has_monitor"
    C_FN = "cron_monitor_has"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
