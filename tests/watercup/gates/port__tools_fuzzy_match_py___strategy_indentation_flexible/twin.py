"""twin: _strategy_indentation_flexible(content, pattern) -> list[tuple[int,int]]."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/fuzzy_match.py"; PY_FN = "_strategy_indentation_flexible"
    C_FN = "fuzzy_match_strategy_indentation_flexible"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); return self._fn(d.get("content",""), d.get("pattern",""))
    def norm(self, c, result):
        c.valstr(_j.dumps([[int(a),int(b)] for a,b in result], separators=(",",":")))
_t=_Twin(); trace=_t.trace; run=_t.run
