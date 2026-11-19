"""twin for tools/browser_tool.py:_terminate_real_profile_chrome (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_terminate_real_profile_chrome"
    C_FN = "_terminate_real_profile_chrome"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
