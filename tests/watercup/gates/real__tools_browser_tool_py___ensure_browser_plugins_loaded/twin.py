"""twin for tools/browser_tool.py:_ensure_browser_plugins_loaded (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_ensure_browser_plugins_loaded"
    C_FN = "browser__ensure_browser_plugins_loaded"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
