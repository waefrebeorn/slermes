"""twin for agent/turn_liveness.py:_warn_invalid_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_liveness.py"
    PY_FN = "_warn_invalid_value"
    C_FN = "_warn_invalid_value"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
