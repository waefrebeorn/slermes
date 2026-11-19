"""twin for agent/relay_runtime.py:_reset_turn_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "_reset_turn_context"
    C_FN = "relay_coordinator_end_turn"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
