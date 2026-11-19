"""twin for gateway/restart_loop_guard.py:clear (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/restart_loop_guard.py"
    PY_FN = "clear"
    C_FN = "restart_loop_clear"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
