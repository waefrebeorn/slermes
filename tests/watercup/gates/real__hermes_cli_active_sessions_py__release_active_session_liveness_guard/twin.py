"""twin for hermes_cli/active_sessions.py:release_active_session_liveness_guard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/active_sessions.py"
    PY_FN = "release_active_session_liveness_guard"
    C_FN = "release_active_session_liveness_guard"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
