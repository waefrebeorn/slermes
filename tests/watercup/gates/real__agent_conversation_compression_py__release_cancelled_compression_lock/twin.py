"""twin for agent/conversation_compression.py:release_cancelled_compression_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "release_cancelled_compression_lock"
    C_FN = "cc_commit_fence_release_cancelled_compression_lock"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
