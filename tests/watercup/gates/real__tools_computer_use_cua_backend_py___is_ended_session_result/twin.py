"""twin for tools/computer_use/cua_backend.py:_is_ended_session_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_is_ended_session_result"
    C_FN = "_is_ended_session_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
