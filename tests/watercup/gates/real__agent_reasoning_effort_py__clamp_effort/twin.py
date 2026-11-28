"""twin: clamp_effort(effort, supported, overrides) -> str|None."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_effort.py"; PY_FN = "clamp_effort"
    C_FN = "clamp_effort"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("effort"), d.get("supported"), d.get("overrides"))
    def norm(self, c, result):
        c.valstr("" if result is None else result)
_t=_Twin(); trace=_t.trace; run=_t.run
