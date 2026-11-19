"""twin for gateway/scale_to_zero.py:_platform_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/scale_to_zero.py"
    PY_FN = "_platform_name"
    C_FN = "gateway_scale_to_zero__platform_name"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
