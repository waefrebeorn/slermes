"""twin for agent/monitoring/cron_health.py:build_cron_health_snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/cron_health.py"
    PY_FN = "build_cron_health_snapshot"
    C_FN = "build_cron_health_snapshot"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
