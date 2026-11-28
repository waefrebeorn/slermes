"""twin: _codex_backend_urls(base_url) -> tuple[str,str,str]."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"; PY_FN = "_codex_backend_urls"
    C_FN = "agent_account_usage_u_codex_backend_urls"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(list(result), separators=(",", ":")))
_t = _Twin(); trace = _t.trace; run = _t.run
