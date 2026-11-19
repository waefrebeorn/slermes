"""twin for cron/monitor.py:_persist_monitor_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/monitor.py"
    PY_FN = "_persist_monitor_state"
    C_FN = "monitor_persist"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
