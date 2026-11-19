"""twin for agent/account_usage.py:AccountUsageSnapshot.available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"
    PY_FN = "AccountUsageSnapshot.available"
    C_FN = "AccountUsageSnapshot.available"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
