"""twin for agent/rate_limit_tracker.py:_safe_float(value, default=0.0).
Input "value\tdefault". Cup a float via format(x,'.17g'); a non-numeric default
(text or otherwise) as str(default)."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/rate_limit_tracker.py"
    PY_FN = "_safe_float"
    C_FN = "agent_rate_limit_tracker_u_safe_float"
    def call(self, inp, inputs, idx=0):
        s = str(inp); val, _, dflt = s.partition("\t")
        # default text -> python literal-ish: numeric if parseable else str
        try:
            df = float(dflt)
        except Exception:
            df = dflt
        return self._fn(val, df)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
