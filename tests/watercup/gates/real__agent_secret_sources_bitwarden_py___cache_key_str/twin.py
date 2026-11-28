"""twin: _cache_key_str port twin."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"; PY_FN = "_cache_key_str"
    C_FN = "agent_secret_sources_bitwarden_u_cache_key_str"
    def call(self, inp, inputs, idx=0):
        return self._fn(tuple(_j.loads(inp)))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
