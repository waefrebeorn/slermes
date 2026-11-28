"""twin: _contrast_ratio(fg, bg) -> float, cup '.17g'."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/journey.py"; PY_FN = "_contrast_ratio"
    C_FN = "hermes_cli_journey_u_contrast_ratio"
    def call(self, inp, inputs, idx=0):
        a, _, b = str(inp).partition(";")
        fg = tuple(int(x) for x in a.split(",")); bg = tuple(int(x) for x in b.split(","))
        return self._fn(fg, bg)
    def norm(self, c, result):
        c.valstr(format(float(result), ".17g"))
_t = _Twin(); trace = _t.trace; run = _t.run
