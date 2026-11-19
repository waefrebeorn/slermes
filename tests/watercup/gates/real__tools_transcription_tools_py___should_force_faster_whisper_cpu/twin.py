"""twin for tools/transcription_tools.py:_should_force_faster_whisper_cpu (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_should_force_faster_whisper_cpu"
    C_FN = "trt_should_force_faster_whisper_cpu"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
