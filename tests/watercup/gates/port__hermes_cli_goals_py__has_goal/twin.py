"""twin for hermes_cli/goals.py:has_goal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/goals.py"
    PY_FN = "has_goal"
    C_FN = "goal_manager_has_goal"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
