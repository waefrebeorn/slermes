"""twin for gateway/scale_to_zero.py:suspend_self (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/scale_to_zero.py"
    PY_FN = "suspend_self"
    C_FN = "suspend_self"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
