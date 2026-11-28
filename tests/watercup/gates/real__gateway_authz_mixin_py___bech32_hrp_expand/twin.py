"""twin: _bech32_hrp_expand."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/authz_mixin.py"; PY_FN = "_bech32_hrp_expand"
    C_FN = "gateway_authz_mixin_u_bech32_hrp_expand"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(result, separators=(",",":")))
_t = _Twin(); trace = _t.trace; run = _t.run
