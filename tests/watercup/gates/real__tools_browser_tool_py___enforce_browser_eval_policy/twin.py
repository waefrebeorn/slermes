"""twin for tools/browser_tool.py:_enforce_browser_eval_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_enforce_browser_eval_policy"
    C_FN = "_enforce_browser_eval_policy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
