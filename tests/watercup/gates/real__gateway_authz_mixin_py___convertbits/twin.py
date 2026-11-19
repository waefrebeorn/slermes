"""twin for gateway/authz_mixin.py:_convertbits (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/authz_mixin.py"
    PY_FN = "_convertbits"
    C_FN = "_convertbits"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
