"""twin: _b64e."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"; PY_FN = "_b64e"
    C_FN = "agent_secret_sources_bitwarden_u_b64e"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp).encode())
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
