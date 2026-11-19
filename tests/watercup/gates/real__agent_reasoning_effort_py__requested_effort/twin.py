"""twin for agent/reasoning_effort.py:requested_effort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_effort.py"
    PY_FN = "requested_effort"
    C_FN = "requested_effort"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
