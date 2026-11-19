"""twin for agent/conversation_compression.py:cancel_before_commit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "cancel_before_commit"
    C_FN = "cc_commit_fence_cancel_before_commit"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
