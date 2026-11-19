"""twin for tools/browser_cdp_tool.py:_browser_cdp_via_supervisor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_cdp_tool.py"
    PY_FN = "_browser_cdp_via_supervisor"
    C_FN = "_browser_cdp_via_supervisor"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
