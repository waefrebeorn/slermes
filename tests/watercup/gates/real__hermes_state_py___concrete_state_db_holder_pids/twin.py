"""twin for hermes_state.py:_concrete_state_db_holder_pids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_concrete_state_db_holder_pids"
    C_FN = "_concrete_state_db_holder_pids"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
