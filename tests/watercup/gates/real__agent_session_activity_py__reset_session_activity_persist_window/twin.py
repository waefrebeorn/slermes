"""twin for agent/session_activity.py:reset_session_activity_persist_window (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/session_activity.py"
    PY_FN = "reset_session_activity_persist_window"
    C_FN = "sa_reset_session_activity_persist_window"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
