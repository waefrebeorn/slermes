"""twin for gateway/run.py:_rescue_orphaned_overflow (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_rescue_orphaned_overflow"
    C_FN = "_rescue_orphaned_overflow"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
