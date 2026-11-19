"""twin for agent/transcription_provider.py:transcribe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transcription_provider.py"
    PY_FN = "transcribe"
    C_FN = "transcribe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
