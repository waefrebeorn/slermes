"""twin for gateway/pairing.py:_secure_write (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"
    PY_FN = "_secure_write"
    C_FN = "_secure_write"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
