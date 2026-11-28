"""twin: is_borrowed_credential_source(source, provider_id)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"; PY_FN = "is_borrowed_credential_source"
    C_FN = "is_borrowed_credential_source"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); return self._fn(d.get("source"), d.get("provider"))
    def norm(self, c, result): c.valstr("True" if result else "False")
_t=_Twin(); trace=_t.trace; run=_t.run
