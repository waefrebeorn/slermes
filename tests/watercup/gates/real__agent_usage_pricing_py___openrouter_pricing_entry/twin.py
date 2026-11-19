"""twin for agent/usage_pricing.py:_openrouter_pricing_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/usage_pricing.py"
    PY_FN = "_openrouter_pricing_entry"
    C_FN = "_openrouter_pricing_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
