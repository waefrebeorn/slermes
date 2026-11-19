"""twin for agent/estop.py:_canonical_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/estop.py"
    PY_FN = "_canonical_root"
    C_FN = "_canonical_root"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
