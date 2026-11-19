"""twin for cron/monitor.py:check_monitor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/monitor.py"
    PY_FN = "check_monitor"
    C_FN = "cron_monitor_check"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
