"""twin: agent/turn_summary.py:format_turn_summary."""
import json as _j
from watercup_twin_runtime import TwinBase
from agent.turn_summary import TurnTally
class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"; PY_FN = "format_turn_summary"
    C_FN = "turn_summary_format"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); t=d.get("tally")
        tally = None if t is None else TurnTally(
            verbs={k: dict(v) for k, v in (t.get("verbs") or {}).items()},
            other_tools=int(t.get("other_tools") or 0),
            lines_added=int(t.get("lines_added") or 0),
            lines_removed=int(t.get("lines_removed") or 0),
            has_line_deltas=bool(t.get("has_line_deltas")))
        return self._fn(float(d.get("elapsed") or 0.0), tally, max_segments=int(d.get("max_segments", 4)))
    def norm(self, c, result): c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
