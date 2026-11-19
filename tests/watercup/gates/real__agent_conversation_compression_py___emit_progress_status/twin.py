"""twin for agent/conversation_compression.py:_emit_progress_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_emit_progress_status"
    C_FN = "_emit_progress_status"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
