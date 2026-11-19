"""twin for agent/monitoring/gateway_health.py:emit_runtime_status_transition (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health.py"
    PY_FN = "emit_runtime_status_transition"
    C_FN = "str_in_list"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
