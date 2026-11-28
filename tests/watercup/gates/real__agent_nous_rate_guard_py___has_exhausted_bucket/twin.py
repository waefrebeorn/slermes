"""twin: _has_exhausted_bucket over the raw input."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/nous_rate_guard.py"; PY_FN = "_has_exhausted_bucket"
    C_FN = "agent_nous_rate_guard_u_has_exhausted_bucket"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
