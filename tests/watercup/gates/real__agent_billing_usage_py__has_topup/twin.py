"""twin for agent/billing_usage.py:has_topup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_usage.py"
    PY_FN = "has_topup"
    C_FN = "has_topup"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
