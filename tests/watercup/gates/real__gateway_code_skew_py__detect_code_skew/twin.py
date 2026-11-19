"""twin for gateway/code_skew.py:detect_code_skew (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/code_skew.py"
    PY_FN = "detect_code_skew"
    C_FN = "detect_code_skew"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
