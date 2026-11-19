"""twin for agent/tool_guardrails.py:halt_decision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"
    PY_FN = "halt_decision"
    C_FN = "halt_decision"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
