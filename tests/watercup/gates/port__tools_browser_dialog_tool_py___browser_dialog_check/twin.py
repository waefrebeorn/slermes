"""twin for tools/browser_dialog_tool.py:_browser_dialog_check (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_dialog_tool.py"
    PY_FN = "_browser_dialog_check"
    C_FN = "_browser_dialog_check"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
