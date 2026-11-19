"""twin for agent/review_idle_queue.py:_ensure_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_idle_queue.py"
    PY_FN = "_ensure_thread"
    C_FN = "_ensure_thread"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
