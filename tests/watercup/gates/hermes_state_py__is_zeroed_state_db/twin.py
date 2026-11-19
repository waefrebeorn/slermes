"""twin for hermes_state.py:is_zeroed_state_db (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "is_zeroed_state_db"
    C_FN = "hermes_state_is_zeroed_state_db"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
