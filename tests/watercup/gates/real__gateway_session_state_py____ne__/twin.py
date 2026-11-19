"""twin for gateway/session_state.py:__ne__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_state.py"
    PY_FN = "__ne__"
    C_FN = "__ne__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
