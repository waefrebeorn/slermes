"""twin for agent/monitoring/gateway_health_export.py:_start_diagnostic_log_streamer (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health_export.py"
    PY_FN = "_start_diagnostic_log_streamer"
    C_FN = "_start_diagnostic_log_streamer"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
