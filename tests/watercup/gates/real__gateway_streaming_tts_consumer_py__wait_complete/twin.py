"""twin for gateway/streaming_tts_consumer.py:wait_complete (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/streaming_tts_consumer.py"
    PY_FN = "wait_complete"
    C_FN = "wait_complete"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
