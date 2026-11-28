"""twin: _bounded_prompt_cache_key."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex.py"; PY_FN = "_bounded_prompt_cache_key"
    C_FN = "agent_transports_codex_u_bounded_prompt_cache_key"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
