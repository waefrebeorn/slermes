"""twin for tools/tts_tool.py:_gemini_model_supports_audio_tags (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_gemini_model_supports_audio_tags"
    C_FN = "tts_gemini_model_supports_audio_tags"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
