"""twin for gateway/run.py:_scale_to_zero_self_suspend (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_scale_to_zero_self_suspend"
    C_FN = "_scale_to_zero_self_suspend"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
