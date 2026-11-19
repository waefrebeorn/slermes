"""twin for agent/review_idle_queue.py:_run (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_idle_queue.py"
    PY_FN = "_run"
    C_FN = "_run"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
