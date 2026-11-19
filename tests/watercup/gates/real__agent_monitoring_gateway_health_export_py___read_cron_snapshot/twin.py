"""twin for agent/monitoring/gateway_health_export.py:_read_cron_snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health_export.py"
    PY_FN = "_read_cron_snapshot"
    C_FN = "_read_cron_snapshot"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
