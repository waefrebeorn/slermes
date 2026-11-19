"""twin for tools/approval.py:reset_current_session_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "reset_current_session_key"
    C_FN = "approval_reset_current_session_key"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
