"""twin for tools/transcription_tools.py:_prepare_audio_for_transcription (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_prepare_audio_for_transcription"
    C_FN = "_prepare_audio_for_transcription"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
