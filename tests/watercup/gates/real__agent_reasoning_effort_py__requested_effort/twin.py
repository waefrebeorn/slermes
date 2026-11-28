"""twin: requested_effort(cfg) -> str|None."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_effort.py"; PY_FN = "requested_effort"
    C_FN = "requested_effort"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("" if result is None else result)
_t=_Twin(); trace=_t.trace; run=_t.run
