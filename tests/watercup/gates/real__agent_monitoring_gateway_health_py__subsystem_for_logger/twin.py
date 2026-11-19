"""twin for agent/monitoring/gateway_health.py:subsystem_for_logger (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health.py"
    PY_FN = "subsystem_for_logger"
    C_FN = "subsystem_for_logger"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
