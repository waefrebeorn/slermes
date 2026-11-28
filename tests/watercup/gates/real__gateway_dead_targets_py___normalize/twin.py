"""twin: gateway/dead_targets.py:_normalize."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/dead_targets.py"; PY_FN = "_normalize"
    C_FN = "dead_target_normalize_key"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); return self._fn(d.get("platform",""), d.get("chat_id",""))
    def norm(self, c, result): c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
