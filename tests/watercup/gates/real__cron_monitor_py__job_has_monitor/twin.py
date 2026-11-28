"""twin: job_has_monitor."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "cron/monitor.py"; PY_FN = "job_has_monitor"
    C_FN = "cron_monitor_u_job_has_monitor"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
