"""twin: _strip_orphan_close_tags(text)."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/think_scrubber.py"; PY_FN = "StreamingThinkScrubber._strip_orphan_close_tags"
    C_FN = "ths_strip_orphan"
    def call(self, inp, inputs, idx=0): return self._fn(inp)
    def norm(self, c, result): c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
