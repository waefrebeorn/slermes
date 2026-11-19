"""twin for agent/monitoring/cron_health.py:_now (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/cron_health.py"
    PY_FN = "_now"
    C_FN = "cron_now"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
