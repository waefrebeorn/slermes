"""twin for agent/error_surface.py:_is_custom_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_surface.py"
    PY_FN = "_is_custom_endpoint"
    C_FN = "_is_custom_endpoint"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
