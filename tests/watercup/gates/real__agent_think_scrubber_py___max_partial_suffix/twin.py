"""twin: _max_partial_suffix(buf, tags)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/think_scrubber.py"; PY_FN = "StreamingThinkScrubber._max_partial_suffix"
    C_FN = "ths_tag_max_partial"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); return self._fn(d.get("buf",""), tuple(d.get("tags") or ()))
    def norm(self, c, result): c.valstr(str(int(result)))
_t=_Twin(); trace=_t.trace; run=_t.run
