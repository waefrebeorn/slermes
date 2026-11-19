"""twin for agent/monitoring/gateway_health_export.py:_redact_string (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health_export.py"
    PY_FN = "_redact_string"
    C_FN = "_redact_string"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
