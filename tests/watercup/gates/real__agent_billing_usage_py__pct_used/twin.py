"""twin for agent/billing_usage.py:pct_used (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_usage.py"
    PY_FN = "pct_used"
    C_FN = "pct_used"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
