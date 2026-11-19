"""twin for agent/billing_usage.py:format_renews (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_usage.py"
    PY_FN = "format_renews"
    C_FN = "format_renews"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
