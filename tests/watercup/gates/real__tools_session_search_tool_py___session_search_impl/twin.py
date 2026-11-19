"""twin for tools/session_search_tool.py:_session_search_impl (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/session_search_tool.py"
    PY_FN = "_session_search_impl"
    C_FN = "_session_search_impl"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
