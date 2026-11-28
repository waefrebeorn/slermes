"""twin for agent/turn_summary.py format_elapsed."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"; PY_FN = "format_elapsed"
    C_FN = "agent_turn_summary_u_format_elapsed"
    def call(self, inp, inputs, idx=0):
        return self._fn(float(str(inp)))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
