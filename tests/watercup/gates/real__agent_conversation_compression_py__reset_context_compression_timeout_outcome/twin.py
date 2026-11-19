"""twin for agent/conversation_compression.py:reset_context_compression_timeout_outcome (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "reset_context_compression_timeout_outcome"
    C_FN = "reset_context_compression_timeout_outcome"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
