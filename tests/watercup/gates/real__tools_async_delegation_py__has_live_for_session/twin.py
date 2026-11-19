"""twin for tools/async_delegation.py:has_live_for_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/async_delegation.py"
    PY_FN = "has_live_for_session"
    C_FN = "async_delegation_has_live_for_session"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
