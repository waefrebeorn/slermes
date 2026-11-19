"""twin for agent/conversation_compression.py:compression_skipped_due_to_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "compression_skipped_due_to_lock"
    C_FN = "cc_compression_skipped_due_to_lock"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
