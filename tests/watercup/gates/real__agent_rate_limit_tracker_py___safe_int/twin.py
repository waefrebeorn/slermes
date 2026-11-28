"""twin for agent/rate_limit_tracker.py:_safe_int(value, default=0).
Input "value\tdefault"."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/rate_limit_tracker.py"
    PY_FN = "_safe_int"
    C_FN = "agent_rate_limit_tracker_u_safe_int"
    def call(self, inp, inputs, idx=0):
        s = str(inp); val, _, dflt = s.partition("\t")
        try:
            dv = int(dflt)
        except Exception:
            dv = dflt
        return self._fn(val, dv)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
