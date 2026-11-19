"""twin for hermes_cli/active_sessions.py:try_acquire_active_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/active_sessions.py"
    PY_FN = "try_acquire_active_session"
    C_FN = "try_acquire_active_session"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
