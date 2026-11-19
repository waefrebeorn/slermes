"""twin for agent/tool_guardrails.py:_sha256 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_guardrails.py"
    PY_FN = "_sha256"
    C_FN = "_sha256"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
