"""twin for hermes_state.py:find_orphaned_gateway_sessions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "find_orphaned_gateway_sessions"
    C_FN = "find_orphaned_gateway_sessions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
