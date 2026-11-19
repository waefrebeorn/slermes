"""twin for gateway/streaming_tts_consumer.py:suppress_whole_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/streaming_tts_consumer.py"
    PY_FN = "suppress_whole_file"
    C_FN = "suppress_whole_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
