"""twin for agent/tool_guardrails.py:_tool_failure_recovery_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"
    PY_FN = "_tool_failure_recovery_hint"
    C_FN = "_tool_failure_recovery_hint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
