"""twin for agent/thinking_timeout_guidance.py:is_thinking_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/thinking_timeout_guidance.py"
    PY_FN = "is_thinking_timeout"
    C_FN = "agent_thinking_timeout_is_thinking_timeout"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
