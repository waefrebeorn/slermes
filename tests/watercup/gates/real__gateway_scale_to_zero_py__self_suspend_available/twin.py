"""twin for gateway/scale_to_zero.py:self_suspend_available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/scale_to_zero.py"
    PY_FN = "self_suspend_available"
    C_FN = "self_suspend_available"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
