"""twin: TurnTally.total_tools (property) over a JSON tally."""
import json as _j, types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"; PY_FN = "TurnTally.total_tools"
    C_FN = "agent_turn_summary_u_total_tools"
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        obj = types.SimpleNamespace(verbs=d.get("verbs") or {}, other_tools=d.get("other_tools", 0))
        return self._fn(obj)
    def norm(self, c, result):
        c.valstr(str(int(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
