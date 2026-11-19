"""twin for hermes_state.py:reset_hygiene_failure_streak (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "reset_hygiene_failure_streak"
    C_FN = "reset_hygiene_failure_streak"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
