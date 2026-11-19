"""twin for cron/monitor.py:build_monitor_diff (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/monitor.py"
    PY_FN = "build_monitor_diff"
    C_FN = "build_monitor_diff"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
