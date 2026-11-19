"""twin for agent/usage_pricing.py:_usage_count (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/usage_pricing.py"
    PY_FN = "_usage_count"
    C_FN = "_usage_count"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
