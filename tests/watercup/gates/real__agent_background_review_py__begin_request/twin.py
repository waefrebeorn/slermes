"""twin for agent/background_review.py:begin_request (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/background_review.py"
    PY_FN = "begin_request"
    C_FN = "begin_request"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
