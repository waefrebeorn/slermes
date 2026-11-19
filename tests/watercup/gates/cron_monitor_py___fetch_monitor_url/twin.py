"""twin for cron/monitor.py:_fetch_monitor_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/monitor.py"
    PY_FN = "_fetch_monitor_url"
    C_FN = "fetch_monitor_url"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
