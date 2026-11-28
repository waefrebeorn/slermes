"""twin: _ensure_contrast(color, background, minimum) -> hex str or None."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/journey.py"; PY_FN = "_ensure_contrast"
    C_FN = "hermes_cli_journey_u_ensure_contrast"
    def call(self, inp, inputs, idx=0):
        s = str(inp); fg, _, rest = s.partition(";"); bg, _, mn = rest.partition(";")
        return self._fn(fg or None, bg, float(mn))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
