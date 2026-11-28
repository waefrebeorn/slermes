"""twin: _sha256 -> hex digest."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"; PY_FN = "_sha256"
    C_FN = "agent_tool_guardrails_u_sha256"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
