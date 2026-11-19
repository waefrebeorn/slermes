"""twin for agent/account_usage.py:build_credits_view (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"
    PY_FN = "build_credits_view"
    C_FN = "build_credits_view"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
