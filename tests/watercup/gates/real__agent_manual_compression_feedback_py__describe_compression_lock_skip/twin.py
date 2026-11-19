"""twin for agent/manual_compression_feedback.py:describe_compression_lock_skip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/manual_compression_feedback.py"
    PY_FN = "describe_compression_lock_skip"
    C_FN = "describe_compression_lock_skip"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
