"""twin for hermes_state.py:_insert_session_row (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_insert_session_row"
    C_FN = "state_usage_insert_session_row"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
