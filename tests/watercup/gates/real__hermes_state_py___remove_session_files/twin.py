"""twin for hermes_state.py:_remove_session_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_remove_session_files"
    C_FN = "_remove_session_files"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
