"""twin for agent/credits_tracker.py:is_free_tier_model (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credits_tracker.py"
    PY_FN = "is_free_tier_model"
    C_FN = "model"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
