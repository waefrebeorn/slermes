"""twin for agent/nous_rate_guard.py:is_genuine_nous_rate_limit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/nous_rate_guard.py"
    PY_FN = "is_genuine_nous_rate_limit"
    C_FN = "nrg_is_genuine_nous_rate_limit"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
