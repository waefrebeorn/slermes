"""twin for gateway/drain_control.py:_marker_epoch_is_stale (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/drain_control.py"
    PY_FN = "_marker_epoch_is_stale"
    C_FN = "drain_control_marker_epoch_is_stale"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
