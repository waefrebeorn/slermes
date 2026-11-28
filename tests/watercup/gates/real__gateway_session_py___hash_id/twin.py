"""twin: _hash_id -> hex digest."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"; PY_FN = "_hash_id"
    C_FN = "gateway_session_u_hash_id"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
