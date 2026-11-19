"""twin for agent/conversation_compression.py:_session_was_rotated_by_compression (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_session_was_rotated_by_compression"
    C_FN = "cc_session_was_rotated_by_compression"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
