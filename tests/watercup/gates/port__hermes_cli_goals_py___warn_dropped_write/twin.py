"""twin for hermes_cli/goals.py:_warn_dropped_write (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/goals.py"
    PY_FN = "_warn_dropped_write"
    C_FN = "goal_warn_dropped_write"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
