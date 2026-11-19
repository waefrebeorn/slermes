"""twin for agent/moa_loop.py:_completed_response_as_stream_chunk (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moa_loop.py"
    PY_FN = "_completed_response_as_stream_chunk"
    C_FN = "_completed_response_as_stream_chunk"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
