"""twin for agent/background_review.py:spawn_background_review_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/background_review.py"
    PY_FN = "spawn_background_review_thread"
    C_FN = "spawn_background_review_thread"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
