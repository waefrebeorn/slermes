"""twin for agent/usage_pricing.py format_cost_label(Decimal)."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/usage_pricing.py"; PY_FN = "format_cost_label"
    C_FN = "agent_usage_pricing_u_format_cost_label"
    def call(self, inp, inputs, idx=0):
        return self._fn(self._mod.Decimal(str(inp)))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
