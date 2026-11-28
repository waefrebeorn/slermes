"""twin: _coerce_int."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"; PY_FN = "_coerce_int"
    C_FN = "gateway_config_u_coerce_int"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("value"), d.get("default"))
    def norm(self, c, result):
        c.valstr(str(int(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
