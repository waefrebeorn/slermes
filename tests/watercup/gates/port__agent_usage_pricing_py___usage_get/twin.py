"""twin for agent/usage_pricing.py:_usage_get (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/usage_pricing.py"
    PY_FN = "_usage_get"
    C_FN = "_usage_get"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
