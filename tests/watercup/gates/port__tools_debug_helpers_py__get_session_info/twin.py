"""twin for tools/debug_helpers.py:get_session_info (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/debug_helpers.py"
    PY_FN = "get_session_info"
    C_FN = "get_session_info"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
