"""twin for gateway/wake.py:_delegation_display_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/wake.py"
    PY_FN = "_delegation_display_metadata"
    C_FN = "_delegation_display_metadata"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
