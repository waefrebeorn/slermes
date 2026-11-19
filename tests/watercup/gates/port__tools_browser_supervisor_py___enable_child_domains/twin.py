"""twin for tools/browser_supervisor.py:_enable_child_domains (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_supervisor.py"
    PY_FN = "_enable_child_domains"
    C_FN = "browser_supervisor_enable_child_domains"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
