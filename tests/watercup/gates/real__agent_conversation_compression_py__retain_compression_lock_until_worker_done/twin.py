"""twin for agent/conversation_compression.py:retain_compression_lock_until_worker_done (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "retain_compression_lock_until_worker_done"
    C_FN = "retain_compression_lock_until_worker_done"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
