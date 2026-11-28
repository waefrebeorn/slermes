"""twin: ToolCallGuardrailController._is_idempotent(tool_name).

Receiver is a real ToolCallGuardrailConfig(), so the IDEMPOTENT_TOOL_NAMES default
is what gets exercised."""
import types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"
    PY_FN = "ToolCallGuardrailController._is_idempotent"
    C_FN = "agent_tool_guardrails_u_is_idempotent"
    def call(self, inp, inputs, idx=0):
        recv = types.SimpleNamespace(config=self._mod.ToolCallGuardrailConfig())
        return self._fn(recv, str(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
