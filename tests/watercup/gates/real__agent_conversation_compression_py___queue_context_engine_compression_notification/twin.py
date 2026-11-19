"""twin for agent/conversation_compression.py:_queue_context_engine_compression_notification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_queue_context_engine_compression_notification"
    C_FN = "_queue_context_engine_compression_notification"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
