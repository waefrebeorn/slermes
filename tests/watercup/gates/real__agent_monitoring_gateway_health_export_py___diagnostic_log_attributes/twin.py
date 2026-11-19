"""twin for agent/monitoring/gateway_health_export.py:_diagnostic_log_attributes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health_export.py"
    PY_FN = "_diagnostic_log_attributes"
    C_FN = "_diagnostic_log_attributes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
