"""twin for gateway/delivery.py:_looks_like_int (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery.py"
    PY_FN = "_looks_like_int"
    C_FN = "looks_like_int"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
