"""twin for tools/browser_tool.py:_start_browser_cleanup_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_start_browser_cleanup_thread"
    C_FN = "browser__start_browser_cleanup_thread"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
