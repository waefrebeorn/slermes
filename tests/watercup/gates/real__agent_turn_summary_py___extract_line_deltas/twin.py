"""twin: _extract_line_deltas(tool_name, result) -> (added,removed) or None."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"; PY_FN = "_extract_line_deltas"
    C_FN = "agent_turn_summary_u_extract_line_deltas"
    def call(self, inp, inputs, idx=0):
        s = str(inp); tool, _, result = s.partition("\t")
        return self._fn(tool, result)
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
