"""twin for gateway/status.py:_is_gateway_runtime_lock_active_strict (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "_is_gateway_runtime_lock_active_strict"
    C_FN = "_is_gateway_runtime_lock_active_strict"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
