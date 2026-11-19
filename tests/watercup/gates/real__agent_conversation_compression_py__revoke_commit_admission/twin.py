"""twin for agent/conversation_compression.py:revoke_commit_admission (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "revoke_commit_admission"
    C_FN = "cc_commit_fence_revoke_commit_admission"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
