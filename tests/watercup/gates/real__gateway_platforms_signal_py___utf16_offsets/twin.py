"""twin for gateway/platforms/signal.py:_utf16_offsets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal.py"
    PY_FN = "_utf16_offsets"
    C_FN = "_utf16_offsets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
