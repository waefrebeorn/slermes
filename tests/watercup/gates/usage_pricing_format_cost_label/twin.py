"""twin for agent/usage_pricing.py:format_cost_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/usage_pricing.py"
    PY_FN = "format_cost_label"
    C_FN = "format_cost_label"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
