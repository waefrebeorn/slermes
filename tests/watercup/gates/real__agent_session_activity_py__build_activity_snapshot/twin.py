"""twin for agent/session_activity.py:build_activity_snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/session_activity.py"
    PY_FN = "build_activity_snapshot"
    C_FN = "build_activity_snapshot"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
