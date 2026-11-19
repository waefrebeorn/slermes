"""twin for agent/conversation_compression.py:conversation_history_after_compression (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "conversation_history_after_compression"
    C_FN = "conversation_history_after_compression"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
