"""twin for agent/moa_loop.py:_degraded_notice(labels list, policy str). Input "labelsJSON|||policy"."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/moa_loop.py"
    PY_FN = "_degraded_notice"
    C_FN = "moa_u_degraded_notice"
    def call(self, inp, inputs, idx=0):
        s = str(inp); lj, _, pol = s.partition("|||")
        return self._fn(_j.loads(lj), pol)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
