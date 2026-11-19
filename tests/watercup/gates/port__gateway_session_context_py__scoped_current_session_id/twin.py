"""twin for gateway/session_context.py:scoped_current_session_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_context.py"
    PY_FN = "scoped_current_session_id"
    C_FN = "scoped_current_session_id"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
