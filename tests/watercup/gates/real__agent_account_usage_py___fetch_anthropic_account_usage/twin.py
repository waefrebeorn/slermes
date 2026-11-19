"""twin for agent/account_usage.py:_fetch_anthropic_account_usage (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"
    PY_FN = "_fetch_anthropic_account_usage"
    C_FN = "_fetch_anthropic_account_usage"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
