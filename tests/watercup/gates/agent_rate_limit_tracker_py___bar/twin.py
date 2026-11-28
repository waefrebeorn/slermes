"""twin for agent/rate_limit_tracker.py:_bar(pct, width) -> UTF-8 bar string."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/rate_limit_tracker.py"
    PY_FN = "_bar"
    C_FN = "agent_rate_limit_tracker_u_bar"
    def call(self, inp, inputs, idx=0):
        v = _j.loads(inp)
        pct = float(v[0]); width = int(v[1])
        return self._fn(pct, width)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
