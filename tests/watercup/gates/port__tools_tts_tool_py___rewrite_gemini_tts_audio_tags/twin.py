"""twin for tools/tts_tool.py:_rewrite_gemini_tts_audio_tags (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_rewrite_gemini_tts_audio_tags"
    C_FN = "tts_rewrite_gemini_tts_audio_tags"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
