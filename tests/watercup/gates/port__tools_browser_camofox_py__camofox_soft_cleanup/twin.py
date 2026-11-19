"""twin for tools/browser_camofox.py:camofox_soft_cleanup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_camofox.py"
    PY_FN = "camofox_soft_cleanup"
    C_FN = "camofox_soft_cleanup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
