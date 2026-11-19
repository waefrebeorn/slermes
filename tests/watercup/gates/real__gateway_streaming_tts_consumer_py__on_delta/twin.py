"""twin for gateway/streaming_tts_consumer.py:on_delta (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/streaming_tts_consumer.py"
    PY_FN = "on_delta"
    C_FN = "on_delta"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
