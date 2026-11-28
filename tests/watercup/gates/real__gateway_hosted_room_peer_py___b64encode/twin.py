"""twin: _b64encode."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_peer.py"; PY_FN = "_b64encode"
    C_FN = "gateway_hosted_room_peer_u_b64encode"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp).encode())
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
