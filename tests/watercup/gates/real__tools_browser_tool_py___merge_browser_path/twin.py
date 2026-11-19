"""twin for tools/browser_tool.py:_merge_browser_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_merge_browser_path"
    C_FN = "_merge_browser_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
