"""twin for tools/browser_tool.py:_allow_unsafe_browser_evaluate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_allow_unsafe_browser_evaluate"
    C_FN = "browser_allow_unsafe_browser_evaluate"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
