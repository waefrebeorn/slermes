"""twin for gateway/browser_control_artifacts.py:__init__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/browser_control_artifacts.py"
    PY_FN = "__init__"
    C_FN = "__init__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
