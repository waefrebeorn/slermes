"""twin for tools/approval.py:is_approval_bypass_active_for_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "is_approval_bypass_active_for_session"
    C_FN = "apr2_is_approval_bypass_active_for_session"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
