"""twin for agent/conversation_compression.py:finish_commit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "finish_commit"
    C_FN = "cc_commit_fence_finish_commit"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
