"""twin for agent/conversation_compression.py:run_compress_context_with_progress_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "run_compress_context_with_progress_timeout"
    C_FN = "run_compress_context_with_progress_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
