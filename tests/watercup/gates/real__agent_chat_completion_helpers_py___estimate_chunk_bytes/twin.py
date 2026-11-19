"""twin for agent/chat_completion_helpers.py:_estimate_chunk_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_estimate_chunk_bytes"
    C_FN = "cch_estimate_chunk_bytes"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
