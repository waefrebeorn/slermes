"""twin for agent/background_review.py:is_background_review_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/background_review.py"
    PY_FN = "is_background_review_enabled"
    C_FN = "is_background_review_enabled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
