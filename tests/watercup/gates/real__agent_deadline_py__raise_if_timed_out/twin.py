"""twin for agent/deadline.py:raise_if_timed_out (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/deadline.py"
    PY_FN = "raise_if_timed_out"
    C_FN = "raise_if_timed_out"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
