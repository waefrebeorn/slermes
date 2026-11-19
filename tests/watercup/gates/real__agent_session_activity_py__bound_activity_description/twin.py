"""twin for agent/session_activity.py:bound_activity_description (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/session_activity.py"
    PY_FN = "bound_activity_description"
    C_FN = "bound_activity_description"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
