"""twin for tools/browser_tool.py:_risky_browser_eval_reason (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_risky_browser_eval_reason"
    C_FN = "_risky_browser_eval_reason"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
