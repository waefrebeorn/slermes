"""twin for agent/review_idle_queue.py:pending_count (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_idle_queue.py"
    PY_FN = "pending_count"
    C_FN = "pending_count"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
