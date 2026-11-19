"""twin for hermes_cli/model_cost_guard.py:_can_trust_pricing_lookup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_cost_guard.py"
    PY_FN = "_can_trust_pricing_lookup"
    C_FN = "_can_trust_pricing_lookup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
