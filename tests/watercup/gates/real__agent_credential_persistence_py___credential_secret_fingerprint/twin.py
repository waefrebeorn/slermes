"""twin: _credential_secret_fingerprint(payload)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"; PY_FN = "_credential_secret_fingerprint"
    C_FN = "credential_secret_fingerprint"
    def call(self, inp, inputs, idx=0): return self._fn(_j.loads(inp))
    def norm(self, c, result): c.valstr("" if result is None else result)
_t=_Twin(); trace=_t.trace; run=_t.run
