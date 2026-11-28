"""twin for agent/rate_limit_tracker.py:_fmt_seconds(float)."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/rate_limit_tracker.py"
    PY_FN = "_fmt_seconds"
    C_FN = "agent_rate_limit_tracker_u_fmt_seconds"
    def call(self, inp, inputs, idx=0):
        return self._fn(float(str(inp)))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
