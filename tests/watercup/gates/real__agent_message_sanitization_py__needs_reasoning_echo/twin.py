"""twin for agent/message_sanitization.py:needs_reasoning_echo (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_sanitization.py"
    PY_FN = "needs_reasoning_echo"
    C_FN = "msg_sanitize_needs_reasoning_echo"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
