"""twin for agent/conversation_compression.py:mark_commit_watermark_fenced (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "mark_commit_watermark_fenced"
    C_FN = "mark_commit_watermark_fenced"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
