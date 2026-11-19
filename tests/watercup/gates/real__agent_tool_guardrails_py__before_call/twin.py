"""twin for agent/tool_guardrails.py:before_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"
    PY_FN = "before_call"
    C_FN = "before_call"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
