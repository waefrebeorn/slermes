"""twin for gateway/streaming_tts_consumer.py:_strip_markdown_for_tts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/streaming_tts_consumer.py"
    PY_FN = "_strip_markdown_for_tts"
    C_FN = "stts_strip_markdown_for_tts"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
