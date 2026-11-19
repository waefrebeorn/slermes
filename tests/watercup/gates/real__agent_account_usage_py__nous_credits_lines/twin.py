"""twin for agent/account_usage.py:nous_credits_lines (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"
    PY_FN = "nous_credits_lines"
    C_FN = "nous_credits_lines"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
