"""twin for agent/credits_tracker.py:evaluate_credits_notices (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credits_tracker.py"
    PY_FN = "evaluate_credits_notices"
    C_FN = "evaluate_credits_notices"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
