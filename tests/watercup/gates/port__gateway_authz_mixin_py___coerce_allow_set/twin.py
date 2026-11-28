"""twin for gateway/authz_mixin.py:_coerce_allow_set (input = raw config text)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/authz_mixin.py"
    PY_FN = "_coerce_allow_set"
    C_FN = "gateway_authz_mixin_u_coerce_allow_set"
    def call(self, inp, inputs, idx=0):
        return self._fn(inp)
    def norm(self, c, result):
        c.valstr(_j.dumps(sorted(str(x) for x in result), separators=(",", ":")))
_t = _Twin(); trace = _t.trace; run = _t.run
