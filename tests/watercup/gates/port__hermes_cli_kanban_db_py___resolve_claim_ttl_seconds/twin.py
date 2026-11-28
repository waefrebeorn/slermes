"""twin: _resolve_claim_ttl_seconds(ttl) -> max(1,int(ttl)) for explicit ttl."""
import json as _j, os as _os
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"; PY_FN = "_resolve_claim_ttl_seconds"
    C_FN = "kanban_resolve_claim_ttl_seconds"
    def call(self, inp, inputs, idx=0):
        _os.environ.pop("HERMES_KANBAN_CLAIM_TTL_SECONDS", None)
        d=_j.loads(inp); return self._fn(d.get("ttl"))
    def norm(self, c, result):
        c.valstr(str(result))
_t=_Twin(); trace=_t.trace; run=_t.run
