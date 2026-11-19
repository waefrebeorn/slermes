"""twin for agent/background_review.py:_record_review_usage_to_parent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/background_review.py"
    PY_FN = "_record_review_usage_to_parent"
    C_FN = "_record_review_usage_to_parent"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
