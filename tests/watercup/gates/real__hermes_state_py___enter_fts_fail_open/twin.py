"""twin for hermes_state.py:_enter_fts_fail_open (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_enter_fts_fail_open"
    C_FN = "_enter_fts_fail_open"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
