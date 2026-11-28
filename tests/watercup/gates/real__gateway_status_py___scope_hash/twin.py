"""twin: _scope_hash -> hex digest."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"; PY_FN = "_scope_hash"
    C_FN = "gateway_status_u_scope_hash"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
