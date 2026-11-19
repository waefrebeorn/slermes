"""twin for agent/conversation_compression.py:_mark_compression_blocked_transient (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_mark_compression_blocked_transient"
    C_FN = "_mark_compression_blocked_transient"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
