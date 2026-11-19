"""twin for agent/turn_liveness.py:_surface_committed_abort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_liveness.py"
    PY_FN = "_surface_committed_abort"
    C_FN = "_surface_committed_abort"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
