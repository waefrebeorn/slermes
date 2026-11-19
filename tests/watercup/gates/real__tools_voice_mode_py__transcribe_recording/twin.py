"""twin for tools/voice_mode.py:transcribe_recording (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "transcribe_recording"
    C_FN = "transcribe_recording"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
