"""twin for tools/transcription_tools.py:transcribe_audio_local_fallback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "transcribe_audio_local_fallback"
    C_FN = "transcribe_audio_local_fallback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
