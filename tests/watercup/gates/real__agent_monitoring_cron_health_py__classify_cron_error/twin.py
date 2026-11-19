"""twin for agent/monitoring/cron_health.py:classify_cron_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/cron_health.py"
    PY_FN = "classify_cron_error"
    C_FN = "classify_cron_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
