"""twin for agent/rate_limit_tracker.py:_fmt_count(int)."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/rate_limit_tracker.py"
    PY_FN = "_fmt_count"
    C_FN = "agent_rate_limit_tracker_u_fmt_count"
    def call(self, inp, inputs, idx=0):
        return self._fn(int(float(str(inp))))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
