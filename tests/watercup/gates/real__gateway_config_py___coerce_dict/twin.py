"""twin: _coerce_dict."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"; PY_FN = "_coerce_dict"
    C_FN = "gateway_config_u_coerce_dict"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(result, separators=(",",":")))
_t = _Twin(); trace = _t.trace; run = _t.run
