"""twin for agent/error_surface.py:build_error_surface_from_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_surface.py"
    PY_FN = "build_error_surface_from_result"
    C_FN = "build_error_surface_from_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
