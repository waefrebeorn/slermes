"""twin for agent/message_sanitization.py:matches_reasoning_echo_family (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_sanitization.py"
    PY_FN = "matches_reasoning_echo_family"
    C_FN = "msg_sanitize_matches_reasoning_echo_family"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
