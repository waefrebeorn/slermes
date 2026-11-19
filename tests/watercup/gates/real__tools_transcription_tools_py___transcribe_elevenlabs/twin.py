"""twin for tools/transcription_tools.py:_transcribe_elevenlabs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_transcribe_elevenlabs"
    C_FN = "_transcribe_elevenlabs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
