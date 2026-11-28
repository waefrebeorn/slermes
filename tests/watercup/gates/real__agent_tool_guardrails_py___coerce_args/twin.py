"""twin: _coerce_args."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"; PY_FN = "_coerce_args"
    C_FN = "agent_tool_guardrails_u_coerce_args"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(result, separators=(",",":")))
_t = _Twin(); trace = _t.trace; run = _t.run
