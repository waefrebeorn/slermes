"""twin for agent/billing_usage.py:usage_model_from_account (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_usage.py"
    PY_FN = "usage_model_from_account"
    C_FN = "usage_model_from_account"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
