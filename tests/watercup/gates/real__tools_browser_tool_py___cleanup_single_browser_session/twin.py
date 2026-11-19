"""twin for tools/browser_tool.py:_cleanup_single_browser_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_cleanup_single_browser_session"
    C_FN = "browser__cleanup_single_browser_session"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
