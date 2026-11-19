"""twin for gateway/streaming_tts_consumer.py:_iter_stream_chunks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/streaming_tts_consumer.py"
    PY_FN = "_iter_stream_chunks"
    C_FN = "_iter_stream_chunks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
