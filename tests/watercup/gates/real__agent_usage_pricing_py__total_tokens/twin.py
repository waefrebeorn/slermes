"""twin for agent/usage_pricing.py CanonicalUsage.total_tokens (property)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/usage_pricing.py"; PY_FN = "CanonicalUsage.total_tokens"
    C_FN = "agent_usage_pricing_u_total_tokens"
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        return self._fn(self._mod.CanonicalUsage(**d))
    def norm(self, c, result):
        c.valstr(str(int(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
