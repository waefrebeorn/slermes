"""twin for agent/monitoring/cron_health.py:_job_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/cron_health.py"
    PY_FN = "_job_key"
    C_FN = "_job_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
