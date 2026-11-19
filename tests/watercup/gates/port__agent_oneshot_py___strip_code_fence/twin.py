"""twin for agent/oneshot.py:_strip_code_fence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/oneshot.py"
    PY_FN = "_strip_code_fence"
    C_FN = "_strip_code_fence"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
