"""twin for agent/verify/environment.py:load_or_detect (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify/environment.py"
    PY_FN = "load_or_detect"
    C_FN = "load_or_detect"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
