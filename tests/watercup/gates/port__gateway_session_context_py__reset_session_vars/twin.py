"""twin for gateway/session_context.py:reset_session_vars (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_context.py"
    PY_FN = "reset_session_vars"
    C_FN = "reset_session_vars"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
