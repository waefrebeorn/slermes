"""twin for tools/computer_use/cua_backend.py:_cua_driver_supports_no_overlay (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_cua_driver_supports_no_overlay"
    C_FN = "cua_u_cua_driver_supports_no_overlay"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
