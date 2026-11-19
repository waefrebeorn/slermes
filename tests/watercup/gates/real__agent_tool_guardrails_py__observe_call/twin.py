"""twin for agent/tool_guardrails.py:observe_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"
    PY_FN = "observe_call"
    C_FN = "observe_call"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
