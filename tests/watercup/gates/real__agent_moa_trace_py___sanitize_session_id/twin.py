"""twin for agent/moa_trace.py:_sanitize_session_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moa_trace.py"
    PY_FN = "_sanitize_session_id"
    C_FN = "_sanitize_session_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
