"""twin: fingerprint_secret_value."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"; PY_FN = "fingerprint_secret_value"
    C_FN = "agent_credential_persistence_u_fingerprint_secret_value"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
