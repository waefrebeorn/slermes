"""twin: _coerce_positive_int(value, default). Input "value\tdefault"."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/tool_output_limits.py"; PY_FN = "_coerce_positive_int"
    C_FN = "tools_tool_output_limits_u_coerce_positive_int"
    def call(self, inp, inputs, idx=0):
        s = str(inp); val, _, dflt = s.partition("\t")
        return self._fn(val, int(dflt or 0))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
