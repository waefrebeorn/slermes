"""twin: _relative_luminance(rgb tuple) -> float, cup '.17g'."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/journey.py"; PY_FN = "_relative_luminance"
    C_FN = "hermes_cli_journey_u_relative_luminance"
    def call(self, inp, inputs, idx=0):
        return self._fn(tuple(int(x) for x in str(inp).split(",")))
    def norm(self, c, result):
        c.valstr(format(float(result), ".17g"))
_t = _Twin(); trace = _t.trace; run = _t.run
