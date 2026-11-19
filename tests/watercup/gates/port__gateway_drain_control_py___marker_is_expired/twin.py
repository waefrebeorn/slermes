"""twin for gateway/drain_control.py:_marker_is_expired (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/drain_control.py"
    PY_FN = "_marker_is_expired"
    C_FN = "_marker_is_expired"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
