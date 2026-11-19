"""twin for tools/tts_streaming.py:mark_speech_interrupted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_streaming.py"
    PY_FN = "mark_speech_interrupted"
    C_FN = "tools_tts_streaming_mark_speech_interrupted"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
