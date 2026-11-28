"""twin: hash_monitor_output -> hex digest."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "cron/monitor.py"; PY_FN = "hash_monitor_output"
    C_FN = "cron_monitor_u_hash_monitor_output"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
