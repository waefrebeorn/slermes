"""twin for agent/monitoring/gateway_health.py:_bounded_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health.py"
    PY_FN = "_bounded_state"
    C_FN = "_bounded_state"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
