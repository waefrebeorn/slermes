"""twin for agent/manual_compression_feedback.py:summarize_manual_compression (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/manual_compression_feedback.py"
    PY_FN = "summarize_manual_compression"
    C_FN = "summarize_manual_compression"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
