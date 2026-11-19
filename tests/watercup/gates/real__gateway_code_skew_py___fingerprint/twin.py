"""twin for gateway/code_skew.py:_fingerprint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/code_skew.py"
    PY_FN = "_fingerprint"
    C_FN = "_fingerprint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
