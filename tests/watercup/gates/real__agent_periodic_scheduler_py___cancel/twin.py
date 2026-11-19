"""twin for agent/periodic_scheduler.py:_cancel (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/periodic_scheduler.py"
    PY_FN = "_cancel"
    C_FN = "_cancel"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
