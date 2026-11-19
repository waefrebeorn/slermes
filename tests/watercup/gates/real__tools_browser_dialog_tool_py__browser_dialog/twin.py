"""twin for tools/browser_dialog_tool.py:browser_dialog (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_dialog_tool.py"
    PY_FN = "browser_dialog"
    C_FN = "browser_dialog"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
