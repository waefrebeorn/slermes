"""twin for agent/relay_runtime.py:_finish_logical_calls (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "_finish_logical_calls"
    C_FN = "relay_coordinator_finish_logical_calls"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
