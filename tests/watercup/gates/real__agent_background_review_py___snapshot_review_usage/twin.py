"""twin for agent/background_review.py:_snapshot_review_usage (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/background_review.py"
    PY_FN = "_snapshot_review_usage"
    C_FN = "_snapshot_review_usage"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
