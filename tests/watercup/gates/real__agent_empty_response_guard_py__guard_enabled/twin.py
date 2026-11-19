"""twin for agent/empty_response_guard.py:guard_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/empty_response_guard.py"
    PY_FN = "guard_enabled"
    C_FN = "guard_enabled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
