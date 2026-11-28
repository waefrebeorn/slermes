"""twin: sanitize_borrowed_credential_payload(payload, provider_id)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"; PY_FN = "sanitize_borrowed_credential_payload"
    C_FN = "sanitize_borrowed_credential_payload"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); return self._fn(d, d.get("provider"))
    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")))
_t=_Twin(); trace=_t.trace; run=_t.run
