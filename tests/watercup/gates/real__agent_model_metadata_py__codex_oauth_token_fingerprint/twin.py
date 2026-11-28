"""twin: _codex_oauth_token_fingerprint."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"; PY_FN = "_codex_oauth_token_fingerprint"
    C_FN = "agent_model_metadata_u_codex_oauth_token_fingerprint"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("access_token",""), d.get("base_url",""))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
