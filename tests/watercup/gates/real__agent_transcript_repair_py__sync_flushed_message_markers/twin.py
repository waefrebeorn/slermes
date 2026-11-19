"""twin for agent/transcript_repair.py:sync_flushed_message_markers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transcript_repair.py"
    PY_FN = "sync_flushed_message_markers"
    C_FN = "sync_flushed_message_markers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
