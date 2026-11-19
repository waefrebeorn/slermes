"""twin for agent/conversation_compression.py:_notify_context_engine_compression_complete (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_notify_context_engine_compression_complete"
    C_FN = "_notify_context_engine_compression_complete"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
