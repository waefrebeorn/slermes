"""twin for agent/conversation_compression.py:mark_context_compression_timed_out (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "mark_context_compression_timed_out"
    C_FN = "mark_context_compression_timed_out"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
