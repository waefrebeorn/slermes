"""twin for hermes_state.py:reclaim_idle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "reclaim_idle"
    C_FN = "reclaim_idle"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
