"""twin for gateway/drain_control.py:_marker_is_stale (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/drain_control.py"
    PY_FN = "_marker_is_stale"
    C_FN = "_marker_is_stale"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
