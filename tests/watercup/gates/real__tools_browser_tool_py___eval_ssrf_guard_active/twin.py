"""twin for tools/browser_tool.py:_eval_ssrf_guard_active (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_eval_ssrf_guard_active"
    C_FN = "browser_eval_ssrf_guard_active"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
